#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include "Editor/Api/PreviewRegistry.h"
#include "Editor/Internals/Diagnostics/EditorDiagnostics.h"
#include "Editor/Internals/Protocol/PreviewProtocol.h"
#include "Editor/Internals/Settings/EditorSettingsStore.h"
#include "Editor/Internals/Viewport/PreviewInputRouter.h"
#include "Platform/FileStorage.h"
#include "Tests/Support/TempDirectory.h"

TEST_CASE("Editor.Catalog.PresentationMetadataIsOptional", "[engine-editor]") {
    PreviewDefinition definition;
    definition.id = "example";
    definition.name = "Example";
    PreviewParameter parameter{"enabled", "Enabled", PreviewParameterKind::Boolean, true};
    parameter.group = "Appearance";
    parameter.description = "Show the overlay.";
    definition.parameters.push_back(parameter);
    definition.actions.push_back({"inspect", "Inspect"});
    definition.actions.push_back({"replay", "Replay", "Restart the sample animation.", true});
    auto encoded = EncodePreviewCatalog({definition});
    const auto decoded = DecodePreviewCatalog(encoded);
    CHECK(decoded.front().parameters.front().group == parameter.group);
    CHECK(decoded.front().parameters.front().description == parameter.description);
    CHECK_FALSE(decoded.front().actions.front().primary);
    CHECK(decoded.front().actions.back().primary);
    CHECK(decoded.front().actions.back().description == definition.actions.back().description);
    auto& entry = encoded["entries"][0];
    entry["parameters"][0].erase("group");
    entry["parameters"][0].erase("description");
    entry["actions"][0].erase("description");
    entry["actions"][1].erase("primary");
    const auto legacy = DecodePreviewCatalog(encoded);
    CHECK(legacy.front().parameters.front().group.empty());
    CHECK(legacy.front().parameters.front().description.empty());
    CHECK(legacy.front().actions.front().description.empty());
    CHECK_FALSE(legacy.front().actions.back().primary);
}

TEST_CASE("Editor.Catalog.PrimaryActionMustBeUnambiguous", "[engine-editor]") {
    PreviewDefinition definition;
    definition.id = "actions";
    definition.name = "Actions";
    definition.actions = {{"inspect", "Inspect", "", true}, {"replay", "Replay", "", true}};
    definition.create = [](const PreviewServices&, const PreviewParameters&) -> std::unique_ptr<EditorPreview> {
        return {};
    };
    PreviewRegistry registry;
    CHECK_THROWS(registry.Add(definition));
    CHECK_THROWS(DecodePreviewCatalog(EncodePreviewCatalog({definition})));
    definition.actions.front().primary = false;
    CHECK_NOTHROW(registry.Add(definition));
    CHECK_NOTHROW(DecodePreviewCatalog(EncodePreviewCatalog({definition})));
}

TEST_CASE("Editor.Diagnostics.BoundsFiltersAndUnreadErrors", "[engine-editor]") {
    EditorDiagnostics diagnostics;
    diagnostics.Append("first\nsecond\r\n", EditorLogLevel::Warning, EditorLogSource::Worker);
    REQUIRE(diagnostics.Entries().size() == 2);
    CHECK(diagnostics.Entries().back().text == "second");
    diagnostics.Append("Build failed", EditorLogLevel::Error, EditorLogSource::Build);
    CHECK(diagnostics.UnreadErrors() == 1);
    EditorLogFilter filter{false, false, true, "FAILED"};
    CHECK(filter.Matches(diagnostics.Entries().back()));
    CHECK_FALSE(filter.Matches(diagnostics.Entries().front()));
    diagnostics.Acknowledge();
    CHECK(diagnostics.UnreadErrors() == 0);
    const auto sequence = diagnostics.Entries().back().sequence;
    diagnostics.Clear();
    diagnostics.Append(std::string(20 * 1024, 'x'), EditorLogLevel::Error);
    CHECK(diagnostics.Entries().back().sequence > sequence);
    CHECK(diagnostics.Entries().back().text.size() == 16 * 1024);
    CHECK(diagnostics.UnreadErrors() == 1);
    for (int i = 0; i < 1100; ++i)
        diagnostics.Append("sample");
    CHECK(diagnostics.Entries().size() == 1000);
    CHECK(diagnostics.UnreadErrors() == 0);
    CHECK(ClassifyBuildOutput("source.cpp(4): warning C4100: unused") == EditorLogLevel::Warning);
    CHECK(ClassifyBuildOutput("source.cpp(4): error C2039: missing") == EditorLogLevel::Error);
    CHECK(ClassifyBuildOutput("CMake Error at CMakeLists.txt:4") == EditorLogLevel::Error);
    CHECK(ClassifyBuildOutput("error count: 0") == EditorLogLevel::Info);
}

TEST_CASE("Editor.Preferences.FavoritesAndSuccessfulOpenHistory", "[engine-editor]") {
    EditorPreferences preferences;
    preferences.ToggleFavorite("a");
    CHECK(preferences.IsFavorite("a"));
    CHECK(preferences.recent.empty());
    preferences.ToggleFavorite("a");
    CHECK_FALSE(preferences.IsFavorite("a"));
    for (int i = 0; i < 15; ++i)
        preferences.Opened(std::to_string(i));
    CHECK(preferences.recent.size() == 10);
    const auto id = preferences.recent.back();
    preferences.Opened(id);
    CHECK(preferences.recent.front() == id);
    CHECK(std::ranges::count(preferences.recent, id) == 1);
    CHECK(preferences.recent.size() == 10);
}

TEST_CASE("Editor.Settings.ProjectIsolationAndCorruptFileRecovery", "[engine-editor]") {
    TempDirectory directory;
    const auto file = directory.Path() / "editor" / "preferences.json";
    EditorSettingsStore store(file);
    EditorPreferences a;
    a.Visible(EditorPanel::Output) = false;
    a.docking = "[Docking][Data]\n";
    a.layout = EditorWorkspaceLayout::Custom;
    a.customDocking = "[Window][Custom]\n";
    a.customVisible[static_cast<std::size_t>(EditorPanel::Catalog)] = false;
    a.lastBottomPanel = EditorPanel::Profiler;
    a.ToggleFavorite("sample");
    a.Opened("sample");
    a.outputFilter.search = "warning";
    a.followOutput = false;
    REQUIRE(store.Save("project-a", a, 150).empty());
    REQUIRE(store.Save("project-b", {}, 125).empty());
    EditorSettingsStore reopened(file);
    EditorPreferences restored;
    int font = 100;
    REQUIRE(reopened.Load("project-a", restored, font).empty());
    CHECK(restored == a);
    CHECK(font == 125);
    REQUIRE(reopened.Load("project-b", restored, font).empty());
    CHECK(restored == EditorPreferences{});
    (void)WriteBinaryFileAtomic(file, "broken json");
    CHECK_FALSE(reopened.Load("project-a", restored, font).empty());
    CHECK(restored == EditorPreferences{});
    CHECK(font == 100);
    REQUIRE(reopened.Save("project-a", a, 200).empty());
    REQUIRE(reopened.Load("project-a", restored, font).empty());
    CHECK(restored == a);
    CHECK(font == 100);
}

TEST_CASE("Editor.Input.HiddenViewportCancelsOwnedInput", "[engine-editor]") {
    PreviewInputRouter router;
    InputSample down;
    down.mousePressed = down.mouseDown = true;
    down.keysDown[static_cast<std::size_t>(InputKey::Enter)] = true;
    const auto viewport = FitPreviewViewport({}, {200, 100}, {400, 200});
    (void)router.Route(down, {50, 50}, viewport, true, true, false, false);
    const auto hidden = router.Route(down, {50, 50}, {}, true, true, false, false);
    CHECK(hidden.cancelled);
    CHECK_FALSE(hidden.mouseReleased);
    CHECK_FALSE(hidden.mouseDown);
    CHECK_FALSE(hidden.Down(InputKey::Enter));
    CHECK_FALSE(router.OwnsPointer());
}

TEST_CASE("Editor.Input.EditorCommandCancelsHeldDevicesUntilNeutral", "[engine-editor]") {
    PreviewInputRouter router;
    const auto viewport = FitPreviewViewport({}, {200, 100}, {400, 200});
    InputSample held;
    held.mousePressed = held.mouseDown = true;
    held.keysDown[static_cast<std::size_t>(InputKey::Enter)] = true;
    (void)router.Route(held, {50, 50}, viewport, true, true, false, false);
    REQUIRE(router.OwnsPointer());
    CHECK(router.Clear().cancelled);
    const auto suppressed = router.Route(held, {50, 50}, viewport, true, true, false, false);
    CHECK_FALSE(suppressed.Down(InputKey::Enter));
    CHECK_FALSE(suppressed.mouseDown);
    CHECK_FALSE(router.OwnsPointer());
    (void)router.Route({}, {50, 50}, viewport, true, true, false, false);
    const auto fresh = router.Route(held, {50, 50}, viewport, true, true, false, false);
    CHECK(fresh.Down(InputKey::Enter));
    CHECK(fresh.mousePressed);
    CHECK(router.OwnsPointer());
}
