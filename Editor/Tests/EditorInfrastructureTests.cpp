#include <catch2/catch_test_macros.hpp>
#include <array>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include "Editor/Api/SceneRegistry.h"
#include "Editor/Internals/Protocol/EditorControlProtocol.h"
#include "Editor/Internals/Protocol/WorkerMessageReader.h"
#include "Editor/Internals/Session/RefreshState.h"
#include "Editor/Internals/Session/WorkerSnapshots.h"
#include "Editor/Internals/Viewport/ViewportInputRouter.h"
#include "Tests/Support/TempDirectory.h"
#ifdef _WIN32
#include "Editor/Internals/Platform/Windows/ControlChannel.h"
#include "Editor/Internals/Platform/Windows/SharedFrames.h"
#include "Editor/Internals/Platform/Windows/WindowsText.h"
#endif

TEST_CASE("Editor.Viewport.LetterboxAndCoordinates", "[engine-editor]") {
    const auto viewport = FitSceneViewport({100.0f, 50.0f}, {1000.0f, 1000.0f}, {1600, 900});
    REQUIRE_FALSE(MapViewportPoint(viewport, {100.0f, 50.0f}));
    const auto center = MapViewportPoint(viewport, {600.0f, 550.0f});
    REQUIRE(center);
    CHECK(*center == Vec2{800, 450});
    CHECK_FALSE(MapViewportPoint(viewport, {viewport.position.x + viewport.size.x, 550.0f}));
    CHECK_FALSE(MapViewportPoint(FitSceneViewport({}, {}, {1600, 900}), {}));
    const auto scaled = FitSceneViewport({200.0f, 100.0f}, {2000.0f, 2000.0f}, {1600, 900});
    CHECK(MapViewportPoint(scaled, {1200.0f, 1100.0f}) == center);
}

TEST_CASE("Editor.Input.OwnedDragReleaseAndFocus", "[engine-editor]") {
    ViewportInputRouter router;
    const auto viewport = FitSceneViewport({}, {200.0f, 100.0f}, {400, 200});
    InputSample down;
    down.mousePressed = true;
    down.mouseDown = true;
    down.keysPressed[static_cast<std::size_t>(InputKey::Enter)] = true;
    down.keysDown = down.keysPressed;
    const auto pressed = router.Route(down, {50.0f, 50.0f}, viewport, true, true, false, false);
    CHECK(pressed.mousePress == Vec2{100, 100});
    CHECK(pressed.Pressed(InputKey::Enter));
    InputSample up;
    up.mouseReleased = true;
    const auto released = router.Route(up, {500.0f, 500.0f}, viewport, true, false, true, true);
    CHECK(released.mouseReleased);
    CHECK_FALSE(released.mousePosition);
    const auto editor = router.Route(down, {50.0f, 50.0f}, viewport, true, false, true, true);
    CHECK_FALSE(editor.Pressed(InputKey::Enter));
    CHECK_FALSE(editor.mousePressed);
    (void)router.Route(down, {50.0f, 50.0f}, viewport, true, true, false, false);
    const auto unfocused = router.Route(down, {50.0f, 50.0f}, viewport, false, true, false, false);
    CHECK(unfocused.cancelled);
    CHECK_FALSE(unfocused.mouseReleased);
    CHECK_FALSE(unfocused.Down(InputKey::Enter));
    CHECK_FALSE(router.Clear().mouseReleased);
}

TEST_CASE("Editor.Input.FocusCancellationDrainsHeldDevicesBeforeAcceptingANewPress", "[engine-editor]") {
    ViewportInputRouter router;
    const auto viewport = FitSceneViewport({}, {200.0f, 100.0f}, {400, 200});
    InputSample down;
    down.mousePressed = down.mouseDown = true;
    down.keysDown[static_cast<std::size_t>(InputKey::Enter)] = true;
    (void)router.Route(down, {50, 50}, viewport, true, true, false, false);
    CHECK(router.Route(down, {50, 50}, viewport, false, true, false, false).cancelled);
    const auto held = router.Route(down, {50, 50}, viewport, true, true, false, false);
    CHECK(held.cancelled);
    CHECK_FALSE(held.mousePressed);
    CHECK_FALSE(held.Down(InputKey::Enter));
    CHECK(router.Route({}, {50, 50}, viewport, true, true, false, false).cancelled);
    const auto next = router.Route(down, {50, 50}, viewport, true, true, false, false);
    CHECK_FALSE(next.cancelled);
    CHECK(next.mousePressed);
}

TEST_CASE("Editor.Viewport.ZoomedImageUsesCanvasCoordinatesAndVisibleClip", "[engine-editor]") {
    // A 2x image is panned behind an 800x600 visible panel.
    const SceneViewport viewport{{-900, -500}, {3200, 1800}, {1600, 900}, {100, 50}, {800, 600}};
    CHECK(MapViewportPoint(viewport, {100, 50}) == Vec2{500, 275});
    CHECK(MapViewportPoint(viewport, {500, 350}) == Vec2{700, 425});
    CHECK_FALSE(MapViewportPoint(viewport, {99, 350}));
    CHECK_FALSE(MapViewportPoint(viewport, {900, 350}));
    ViewportInputRouter router;
    InputSample down;
    down.mousePressed = true;
    down.mouseDown = true;
    CHECK(router.Route(down, {500, 350}, viewport, true, true, false, false).mousePress == Vec2{700, 425});
    InputSample up;
    up.mouseReleased = true;
    const auto release = router.Route(up, {901, 350}, viewport, true, true, false, true);
    CHECK(release.mouseReleased);
    CHECK_FALSE(release.mousePosition);
}

TEST_CASE("Editor.Protocol.InspectionIsReadOnlyAndRejectsInvalidGeometryAndRows", "[engine-editor]") {
    SceneInspection inspection;
    inspection.focus = SceneContentBounds{320, 250, 960, 400};
    inspection.values = {{"State", "Playing", SceneReadoutKind::Status},
                         {"Count", "12", SceneReadoutKind::Number},
                         {"Note", "A description that can wrap over multiple lines."}};
    inspection.tables = {{"Schedule", {"World", "Boss"}, {{"5", "Sample"}}}};
    inspection.hiddenParameters = {"inactive"};
    inspection.disabledParameters = {"fixed"};
    const auto encoded = EncodeSceneInspection(inspection);
    CHECK(DecodeSceneInspection(encoded) == inspection);
    CHECK_FALSE(encoded.contains("parameters"));
    auto invalid = encoded;
    invalid["focus"] = {320, 250, -1, 400};
    CHECK_THROWS(DecodeSceneInspection(invalid));
    invalid = encoded;
    invalid["tables"][0]["rows"][0] = {"missing column"};
    CHECK_THROWS(DecodeSceneInspection(invalid));
    invalid = encoded;
    invalid["values"][0][2] = 99;
    CHECK_THROWS(DecodeSceneInspection(invalid));
    invalid["values"][0][2] = 1.5;
    CHECK_THROWS(DecodeSceneInspection(invalid));
    auto legacy = encoded;
    legacy["values"][0].erase(legacy["values"][0].begin() + 2);
    CHECK(DecodeSceneInspection(legacy).values.front().kind == SceneReadoutKind::Text);
}

TEST_CASE("Editor.Input.SnapshotsPreserveEdges", "[engine-editor]") {
    DeviceInput state;
    DeviceInput down;
    down.mousePressed = true;
    down.mouseDown = true;
    down.pressPoints = {{20, 30}};
    down.touches = {{7, Vec2{20, 30}}};
    down.keysPressed[0] = true;
    down.keysDown[0] = true;
    DeviceInput up;
    up.mouseReleased = true;
    up.touches = {{7, std::nullopt}};
    AccumulateViewportInput(state, down);
    AccumulateViewportInput(state, up);
    const auto frame = ConsumeViewportInput(state);
    CHECK(frame.mousePressed);
    CHECK(frame.mouseReleased);
    CHECK_FALSE(frame.mouseDown);
    CHECK(frame.keysPressed[0]);
    CHECK_FALSE(frame.keysDown[0]);
    REQUIRE(frame.pressPoints.size() == 1);
    CHECK(frame.pressPoints.front() == Vec2{20, 30});
    REQUIRE(frame.touches.size() == 1);
    CHECK(frame.touches.front().id == 7);
    CHECK_FALSE(frame.touches.front().position);
    const auto next = ConsumeViewportInput(state);
    CHECK_FALSE(next.keysPressed[0]);
    CHECK_FALSE(next.mouseReleased);
    CHECK(next.pressPoints.empty());
}

TEST_CASE("Editor.Input.ProtocolPreservesTouchIdentityAndRejectsAmbiguousContacts", "[engine-editor]") {
    DeviceInput input;
    input.keysDown[static_cast<std::size_t>(InputKey::RightShift)] = true;
    input.mousePosition = Vec2{100, 120};
    input.mousePress = Vec2{90, 110};
    input.mouseHover = input.mousePosition;
    input.mousePressed = true;
    input.mouseReleased = true;
    input.wheel = -0.5f;
    input.pressPoints = {{90, 110}, {150, 160}};
    input.heldPoints = {{160, 170}};
    input.touches = {{7, std::nullopt}, {9, Vec2{160, 170}}};
    const auto encoded = EncodeWorkerInput(input);
    const auto decoded = DecodeWorkerInput(encoded);
    CHECK(decoded.ShiftDown());
    CHECK(decoded.mousePress == input.mousePress);
    CHECK(decoded.mouseHover == input.mouseHover);
    CHECK(decoded.mousePosition == input.mousePosition);
    CHECK(decoded.mousePressed);
    CHECK(decoded.mouseReleased);
    CHECK(decoded.pressPoints == input.pressPoints);
    CHECK(decoded.heldPoints == input.heldPoints);
    REQUIRE(decoded.touches.size() == 2);
    CHECK(decoded.touches[0].id == 7);
    CHECK_FALSE(decoded.touches[0].position);
    CHECK(decoded.touches[1].id == 9);
    CHECK(decoded.touches[1].position == Vec2{160, 170});
    auto invalid = encoded;
    invalid["touches"][1]["id"] = 7;
    CHECK_THROWS(DecodeWorkerInput(invalid));
    invalid = encoded;
    invalid["touches"][0]["id"] = std::numeric_limits<std::uint64_t>::max();
    CHECK_THROWS(DecodeWorkerInput(invalid));
    invalid = encoded;
    invalid["pressed"].push_back(false);
    CHECK_THROWS(DecodeWorkerInput(invalid));
}

TEST_CASE("Editor.Input.TouchOwnershipUsesTheSameCanvasContractAsMouse", "[engine-editor]") {
    ViewportInputRouter router;
    const auto viewport = FitSceneViewport({}, {200.0f, 100.0f}, {400, 200});
    InputSample sample;
    sample.touches = {{7, {50, 60}}};
    auto input = router.Route(sample, {}, viewport, true, true, false, false);
    REQUIRE(input.touches.size() == 1);
    CHECK(input.touches.front().id == 7);
    CHECK(input.touches.front().position == Vec2{100, 120});
    REQUIRE(input.pressPoints.size() == 1);
    CHECK(input.pressPoints.front() == Vec2{100, 120});
    CHECK(router.OwnsPointer());
    sample.touches[0].position = {250, 60};
    input = router.Route(sample, {}, viewport, true, true, false, true);
    REQUIRE(input.touches.size() == 1);
    CHECK_FALSE(input.touches.front().position);
    CHECK(input.pressPoints.empty());
    (void)router.Route({}, {}, viewport, true, true, false, false);
    CHECK_FALSE(router.OwnsPointer());
    sample.touches = {{9, {250, 60}}};
    CHECK(router.Route(sample, {}, viewport, true, true, false, false).touches.empty());
    sample.touches[0].position = {50, 60};
    CHECK(router.Route(sample, {}, viewport, true, true, false, false).touches.empty());
}

TEST_CASE("Editor.Input.CancellationDiscardsOldEdgesAndPrecedesNewInput", "[engine-editor]") {
    DeviceInput state;
    DeviceInput old;
    old.keysPressed[0] = old.keysDown[0] = true;
    old.mousePressed = old.mouseDown = true;
    old.pressPoints = {{20, 30}};
    AccumulateViewportInput(state, old);
    const DeviceInput cancel{.cancelled = true};
    const auto decoded = DecodeWorkerInput(EncodeWorkerInput(cancel));
    REQUIRE(decoded.cancelled);
    AccumulateViewportInput(state, decoded);
    DeviceInput next;
    next.keysPressed[1] = next.keysDown[1] = true;
    next.mousePressed = next.mouseDown = true;
    next.mousePosition = Vec2{40, 50};
    next.pressPoints = {{40, 50}};
    AccumulateViewportInput(state, next);
    const auto barrier = ConsumeViewportInput(state);
    CHECK(barrier.cancelled);
    CHECK_FALSE(barrier.keysPressed[0]);
    CHECK_FALSE(barrier.keysPressed[1]);
    CHECK(barrier.pressPoints.empty());
    const auto fresh = ConsumeViewportInput(state);
    CHECK_FALSE(fresh.cancelled);
    CHECK_FALSE(fresh.keysPressed[0]);
    CHECK(fresh.keysPressed[1]);
    CHECK(fresh.mousePressed);
    REQUIRE(fresh.pressPoints.size() == 1);
    CHECK(fresh.pressPoints.front() == Vec2{40, 50});
    CHECK_FALSE(ConsumeViewportInput(state).keysPressed[1]);
}

TEST_CASE("Editor.Protocol.FragmentationAndSessionIsolation", "[engine-editor]") {
    const WorkerMessage message{"session-a", 3, 7, "pause", {{"paused", true}}};
    const auto bytes = EncodeWorkerMessage(message);
    WorkerMessageReader reader("session-a");
    CHECK(reader.Push(std::span(bytes).first(2)).empty());
    CHECK(reader.Push(std::span(bytes).subspan(2, 3)).empty());
    const auto received = reader.Push(std::span(bytes).subspan(5));
    REQUIRE(received.size() == 1);
    CHECK(received.front().generation == 3);
    CHECK(received.front().request == 7);
    WorkerMessageReader wrongSession("session-b");
    CHECK_THROWS(wrongSession.Push(bytes));
    auto invalid = nlohmann::json{{"version", WorkerProtocolVersion + 1},
                                  {"session", "session-a"},
                                  {"generation", 3},
                                  {"request", 7},
                                  {"type", "pause"},
                                  {"data", message.data}};
    CHECK_THROWS(DecodeWorkerMessage(invalid, "session-a"));
    const std::array oversized{std::byte{0xff}, std::byte{0xff}, std::byte{0xff}, std::byte{0x7f}};
    WorkerMessageReader bounded("session-a");
    CHECK_THROWS(bounded.Push(oversized));
}

TEST_CASE("Editor.Parameters.ValidationAndCompatibleRestore", "[engine-editor]") {
    SceneDefinition definition;
    definition.parameters = {{"radius", "Radius", SceneParameterKind::Float, 10.0, 1.0, 20.0},
                             {"enabled", "Enabled", SceneParameterKind::Boolean, true}};
    CHECK(ValidateSceneValue(definition.parameters[0], 15.0).empty());
    CHECK_FALSE(ValidateSceneValue(definition.parameters[0], std::int64_t{15}).empty());
    CHECK_FALSE(ValidateSceneValue(definition.parameters[0], std::numeric_limits<double>::infinity()).empty());
    SceneParameters saved{{"radius", 15.0}, {"enabled", std::string("old type")}, {"removed", true}};
    std::vector<std::string> messages;
    const auto restored = RestoreSceneParameters(definition, saved, messages);
    CHECK(std::get<double>(restored.at("radius")) == 15.0);
    CHECK(std::get<bool>(restored.at("enabled")));
    CHECK_FALSE(restored.contains("removed"));
    CHECK_FALSE(messages.empty());
    CHECK(DecodeSceneParameters(EncodeSceneParameters(restored)) == restored);
    CHECK_THROWS(DecodeSceneValue({{"type", 3}, {"value", {1.0, -1.0, 0.0, 1.0}}}));
}

TEST_CASE("Editor.Parameters.CapturedControlsAreAtomicAndPreserveResetRecipe", "[engine-editor]") {
    SceneDefinition definition;
    definition.parameters = {
        {"radius", "Radius", SceneParameterKind::Float, 10.0, 1.0, 20.0},
        {"copies", "Copies", SceneParameterKind::Integer, std::int64_t{1}, 1.0, 4.0, SceneApplyMode::NextAction},
        {"seed", "Seed", SceneParameterKind::Integer, std::int64_t{7}, 0.0, 100.0, SceneApplyMode::OnReset}};
    auto accepted = SceneDefaults(definition);
    accepted["seed"] = std::int64_t{42};
    const SceneParameters capture{{"radius", 15.0}, {"copies", std::int64_t{3}}, {"seed", std::int64_t{7}}};
    CHECK(MergeSceneCapture(definition, capture, accepted));
    CHECK(std::get<double>(accepted.at("radius")) == 15.0);
    CHECK(std::get<std::int64_t>(accepted.at("copies")) == 3);
    CHECK(std::get<std::int64_t>(accepted.at("seed")) == 42);
    CHECK_FALSE(MergeSceneCapture(definition, capture, accepted));
    const auto before = accepted;
    CHECK_THROWS(MergeSceneCapture(definition, {{"copies", std::int64_t{2}}, {"radius", 25.0}}, accepted));
    CHECK(accepted == before);
    CHECK_THROWS(MergeSceneCapture(definition, {{"removed", true}}, accepted));
    CHECK(accepted == before);
    CHECK(DecodeSceneParameters(EncodeSceneParameters(accepted)) == accepted);
}

TEST_CASE("Editor.Registry.MetadataDoesNotConstructContent", "[engine-editor]") {
    SceneRegistry registry;
    bool constructed = false;
    SceneDefinition definition;
    definition.id = "test.scene";
    definition.name = "Test";
    definition.categoryOrder = -1;
    definition.parameters = {{"type",
                              "Type",
                              SceneParameterKind::Choice,
                              std::string("First"),
                              0,
                              1,
                              SceneApplyMode::Immediate,
                              {"First", "Second"}}};
    definition.parameters.front().choiceButtons = true;
    definition.parameters.front().choiceButtonColumns = 2;
    definition.parameters.push_back({"radius", "Radius", SceneParameterKind::Float, 10.0, 1.0, 20.0});
    definition.parameters.back().numericInput = true;
    definition.parameters.back().advanced = true;
    definition.parameters.back().apply = SceneApplyMode::NextAction;
    definition.parameters.back().applyAction = "replay";
    definition.actions = {{"replay", "Replay", "Apply the prepared values.", true}};
    definition.create = [&constructed](const SceneServices&, const SceneParameters&) -> std::unique_ptr<EditorScene> {
        constructed = true;
        return {};
    };
    registry.Add(definition);
    const auto catalog = DecodeSceneCatalog(EncodeSceneCatalog(registry.Entries()));
    CHECK_FALSE(constructed);
    REQUIRE_FALSE(catalog.empty());
    CHECK(catalog.front().id == definition.id);
    CHECK(catalog.front().categoryOrder == definition.categoryOrder);
    REQUIRE(catalog.front().parameters.size() == 2);
    CHECK(catalog.front().parameters.front().choiceButtons);
    CHECK(catalog.front().parameters.front().choiceButtonColumns == 2);
    CHECK(catalog.front().parameters.back().numericInput);
    CHECK(catalog.front().parameters.back().advanced);
    CHECK(catalog.front().parameters.back().applyAction == "replay");
    CHECK(catalog.front().actions.front().primary);
    auto invalid = EncodeSceneCatalog(registry.Entries());
    invalid["entries"][0]["parameters"][1]["applyAction"] = "missing";
    CHECK_THROWS(DecodeSceneCatalog(invalid));
    invalid = EncodeSceneCatalog(registry.Entries());
    invalid["entries"][0]["parameters"][1]["apply"] = SceneApplyMode::Immediate;
    CHECK_THROWS(DecodeSceneCatalog(invalid));
    auto legacy = EncodeSceneCatalog(registry.Entries());
    legacy["entries"][0].erase("categoryOrder");
    CHECK(DecodeSceneCatalog(legacy).front().categoryOrder == 0);
    legacy["entries"][0]["parameters"][0].erase("choiceButtons");
    CHECK_FALSE(DecodeSceneCatalog(legacy).front().parameters.front().choiceButtons);
    legacy["entries"][0]["parameters"][0].erase("choiceButtonColumns");
    CHECK(DecodeSceneCatalog(legacy).front().parameters.front().choiceButtonColumns == 0);
    legacy["entries"][0]["parameters"][1].erase("numericInput");
    CHECK_FALSE(DecodeSceneCatalog(legacy).front().parameters.back().numericInput);
    legacy["entries"][0]["parameters"][1].erase("advanced");
    legacy["entries"][0]["parameters"][1].erase("applyAction");
    const auto defaults = DecodeSceneCatalog(legacy);
    CHECK_FALSE(defaults.front().parameters.back().advanced);
    CHECK(defaults.front().parameters.back().applyAction.empty());
    CHECK_FALSE(catalog.front().create);
    CHECK_THROWS(registry.Add(definition));
}

TEST_CASE("Editor.Refresh.BuildThenStartBesideTheRunningScene", "[engine-editor]") {
    RefreshState state;
    CHECK_THROWS(state.BuildFinished(true));
    state.Begin();
    CHECK(state.Building());
    CHECK_THROWS(state.Begin());
    CHECK_THROWS(state.Started());
    state.BuildFinished(false);
    CHECK_FALSE(state.Busy()); // A failed build leaves the running scene as it was
    state.Begin();
    state.BuildFinished(true);
    CHECK(state.Phase() == RefreshPhase::Starting);
    CHECK_FALSE(state.Building());
    state.Started();
    CHECK_FALSE(state.Busy());
    state.Begin();
    state.Cancel();
    CHECK_FALSE(state.Busy());
}

TEST_CASE("Editor.Snapshots.WorkersRunFromCopiesOfTheBuild", "[engine-editor]") {
    const TempDirectory directory;
    ProcessCommand built;
    built.executable = directory.Path() / "MyGameWorker.exe";
    built.directory = directory.Path();
    WorkerSnapshots snapshots(built);
    CHECK_FALSE(snapshots.BuildAvailable());
    CHECK_THROWS(snapshots.Capture());
    CHECK_FALSE(snapshots.Current());

    const auto write = [](const std::filesystem::path& file, const std::string& text) {
        std::ofstream(file, std::ios::binary) << text;
    };
    const auto read = [](const std::filesystem::path& file) {
        std::ifstream input(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), {});
    };
    write(built.executable, "first");
    snapshots.Capture();
    const auto first = snapshots.Current()->executable;
    CHECK(first != built.executable);
    CHECK(first.parent_path() == built.executable.parent_path()); // Assets resolve as for the build output
    CHECK(snapshots.Current()->directory == built.directory);
    CHECK(read(first) == "first");

    write(built.executable, "second"); // The next build overwrites only the output
    snapshots.Capture();
    const auto second = snapshots.Current()->executable;
    CHECK(second != first);
    CHECK(read(first) == "first");
    CHECK(read(second) == "second");

    write(directory.Path() / "Unrelated.live-3.exe", "other");
    snapshots.Prune();
    CHECK_FALSE(std::filesystem::exists(first));
    CHECK(std::filesystem::exists(second));
    CHECK(std::filesystem::exists(built.executable));
    CHECK(std::filesystem::exists(directory.Path() / "Unrelated.live-3.exe"));
}

TEST_CASE("Editor.Control.RequestsRepliesAndAnnouncement", "[engine-editor]") {
    const auto version = std::to_string(EditorControlVersion);
    CHECK(DecodeControlRequest(R"({"version": )" + version + R"(, "command": "refresh"})").command ==
          EditorControlCommand::Refresh);
    const auto seek = DecodeControlRequest(R"({"version": )" + version + R"(, "command": "seek", "seconds": 1.5})");
    CHECK(seek.command == EditorControlCommand::Seek);
    CHECK(seek.arguments == nlohmann::json{{"seconds", 1.5}}); // Without version and command
    for (int i = 0; i < static_cast<int>(EditorControlCommand::Count); ++i) {
        const auto command = static_cast<EditorControlCommand>(i);
        const auto line = nlohmann::json{{"version", EditorControlVersion}, {"command", ControlCommandName(command)}};
        CHECK(DecodeControlRequest(line.dump()).command == command);
    }
    CHECK_THROWS(DecodeControlRequest(R"({"version": 1, "command": "refresh"})"));
    CHECK_THROWS(DecodeControlRequest(R"({"version": )" + version + R"(, "command": "format-disk"})"));
    CHECK_THROWS(DecodeControlRequest("not json"));
    const auto reply = nlohmann::json::parse(EncodeControlReply({{"path", "C:/shot.png"}}));
    CHECK(reply.at("type") == "result");
    CHECK(reply.at("path") == "C:/shot.png");

    const auto result = nlohmann::json::parse(EncodeControlResult(EditorBuildResult::BuildFailed));
    CHECK(result.at("type") == "result");
    CHECK(result.at("result") == "build-failed");
    CHECK_THROWS(EncodeControlResult(EditorBuildResult::Running));
    EditorLogEntry entry;
    entry.level = EditorLogLevel::Error;
    entry.source = EditorLogSource::Build;
    entry.text = "MyGame.cpp(12): error C2065";
    const auto log = nlohmann::json::parse(EncodeControlLog(entry));
    CHECK(log.at("level") == "error");
    CHECK(log.at("source") == "build");
    CHECK(log.at("text") == entry.text);
    CHECK(EncodeControlLog(entry).find('\n') == std::string::npos); // One reply per line

    const auto announcement = DecodeControlAnnouncement(EncodeControlAnnouncement({R"(\\.\pipe\x)", 42}));
    CHECK(announcement.pipe == R"(\\.\pipe\x)");
    CHECK(announcement.processId == 42);
}

TEST_CASE("Editor.Protocol.OpenStateRoundTripsAndRejectsInvalidSpeed", "[engine-editor]") {
    const WorkerOpenState state{"drawing.circle", {{"radius", 24.0}}, true, 2.0};
    const auto decoded = DecodeWorkerOpen(EncodeWorkerOpen(state));
    CHECK(decoded.id == state.id);
    CHECK(decoded.parameters == state.parameters);
    CHECK(decoded.paused);
    CHECK(decoded.speed == 2.0);
    auto invalid = EncodeWorkerOpen(state);
    invalid["speed"] = WorkerMaxPlaybackSpeed * 2;
    CHECK_THROWS(DecodeWorkerOpen(invalid));
}

#ifdef _WIN32
TEST_CASE("Editor.Channel.CatalogAcknowledgementBeforeClose", "[engine-editor]") {
    const auto session = NewWorkerSession();
    ControlChannel server(session, true);
    CHECK(server.Poll().empty()); // No client: the poll must return rather than wait.
    ControlChannel client(session, false);
    client.Send({{}, 0, 0, "catalog", {{"entries", nlohmann::json::array()}}});
    (void)client.Poll();
    const auto catalog = server.Poll();
    REQUIRE(catalog.size() == 1);
    CHECK(catalog.front().type == "catalog");
    server.Send({{}, 0, 1, "catalog-ack", nlohmann::json::object()});
    (void)server.Poll();
    const auto acknowledged = client.Poll();
    REQUIRE(acknowledged.size() == 1);
    CHECK(acknowledged.front().type == "catalog-ack");
    CHECK_FALSE(client.Closed());
}

TEST_CASE("Editor.Frames.LatestCompleteGenerationOnly", "[engine-editor]") {
    const auto session = NewWorkerSession();
    SharedFrames producer(session, true);
    SharedFrames consumer(session, false);
    std::array<std::byte, 16> pixels{};
    pixels[0] = std::byte{0x81};
    REQUIRE(producer.Publish(1, 2, 2, pixels));
    CHECK_FALSE(consumer.Read(2));
    const auto first = consumer.Read(1);
    REQUIRE(first);
    CHECK(first->width == 2);
    CHECK(first->rgba.front() == std::byte{0x81});
    CHECK_FALSE(consumer.Read(1));
    REQUIRE(producer.Publish(1, 2, 2, pixels));
    pixels[0] = std::byte{0x42};
    REQUIRE(producer.Publish(2, 2, 2, pixels));
    CHECK_FALSE(consumer.Read(1));
    const auto latest = consumer.Read(2);
    REQUIRE(latest);
    CHECK(latest->sequence > first->sequence);
    CHECK(latest->rgba.front() == std::byte{0x42});
    CHECK_THROWS(producer.Publish(2, WorkerMaxFrameWidth + 1, 2, pixels));
}
#endif
