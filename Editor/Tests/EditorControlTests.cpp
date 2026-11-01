#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "Editor/Internals/Protocol/EditorControlProtocol.h"
#include "Editor/Internals/Session/FrameCapture.h"
#include "Editor/Internals/Session/ViewportMark.h"
#include "Tests/Support/TempDirectory.h"

namespace {
SceneParameter Parameter(SceneParameterKind kind) {
    SceneParameter parameter;
    parameter.id = "value";
    parameter.kind = kind;
    return parameter;
}
// A frame whose pixel (x, y) is (x, y, 0, 255).
ViewportPixels Gradient(int width, int height) {
    ViewportPixels frame;
    frame.width = width;
    frame.height = height;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            for (const int channel : {x, y, 0, 255})
                frame.rgba.push_back(static_cast<std::byte>(channel));
    return frame;
}
int Channel(const ViewportPixels& frame, int x, int y, int channel) {
    return std::to_integer<int>(frame.rgba[static_cast<std::size_t>((y * frame.width + x) * 4 + channel)]);
}
} // namespace

TEST_CASE("Editor.Control.ValuesFollowTheParameterKind", "[engine-editor]") {
    CHECK(ControlValueOf(Parameter(SceneParameterKind::Integer), 3) == SceneValue{std::int64_t{3}});
    CHECK(ControlValueOf(Parameter(SceneParameterKind::Integer), 4.0) == SceneValue{std::int64_t{4}});
    CHECK_THROWS(ControlValueOf(Parameter(SceneParameterKind::Integer), 4.5));
    CHECK(ControlValueOf(Parameter(SceneParameterKind::Float), 2) == SceneValue{2.0});
    CHECK_THROWS(ControlValueOf(Parameter(SceneParameterKind::Float), "2"));
    CHECK(ControlValueOf(Parameter(SceneParameterKind::Boolean), true) == SceneValue{true});
    CHECK_THROWS(ControlValueOf(Parameter(SceneParameterKind::Boolean), 1));
    CHECK(ControlValueOf(Parameter(SceneParameterKind::Choice), "Fast") == SceneValue{std::string("Fast")});
    CHECK(ControlValueOf(Parameter(SceneParameterKind::Color), nlohmann::json::array({1, 0, 0})) ==
          SceneValue{SceneColor{1.0f, 0.0f, 0.0f, 1.0f}});
    CHECK(ControlValueOf(Parameter(SceneParameterKind::Color), "#ff000080") ==
          SceneValue{SceneColor{1.0f, 0.0f, 0.0f, 128.0f / 255.0f}});
    CHECK_THROWS(ControlValueOf(Parameter(SceneParameterKind::Color), "red"));
    CHECK_THROWS(ControlValueOf(Parameter(SceneParameterKind::Color), nlohmann::json::array({1, 0})));
    // Values go back out as plain JSON
    CHECK(ControlValue(SceneColor{0.0f, 1.0f, 0.0f, 1.0f}) == nlohmann::json::array({0.0f, 1.0f, 0.0f, 1.0f}));
    CHECK(ControlValue(std::int64_t{7}) == 7);
}

TEST_CASE("Editor.Control.CatalogEntryShowsCurrentValuesAndActions", "[engine-editor]") {
    SceneDefinition definition;
    definition.id = "drawing.circle";
    definition.name = "Circle";
    auto radius = Parameter(SceneParameterKind::Float);
    radius.id = "radius";
    radius.initial = 10.0;
    radius.maximum = 50.0;
    auto mode = Parameter(SceneParameterKind::Choice);
    mode.id = "mode";
    mode.initial = std::string("Slow");
    mode.choices = {"Slow", "Fast"};
    definition.parameters = {radius, mode};
    definition.actions = {{"spawn", "Spawn"}};
    const auto entry = ControlCatalogEntry(definition, {{"radius", 24.0}});
    CHECK(entry.at("id") == "drawing.circle");
    CHECK(entry.at("parameters")[0].at("value") == 24.0); // The current value, not the initial one
    CHECK(entry.at("parameters")[0].at("maximum") == 50.0);
    CHECK(entry.at("parameters")[1].at("value") == "Slow");
    CHECK(entry.at("parameters")[1].at("choices") == nlohmann::json::array({"Slow", "Fast"}));
    CHECK(entry.at("actions")[0].at("id") == "spawn");
}

TEST_CASE("Editor.Capture.CropsInCanvasUnitsAndScales", "[engine-editor]") {
    const auto frame = Gradient(8, 4);
    const auto whole = CaptureFrame(frame, {8, 4}, {});
    CHECK(whole.width == 8);
    CHECK(whole.height == 4);
    CHECK(whole.rgba == frame.rgba);

    const auto crop = CaptureFrame(frame, {8, 4}, {FloatRect{2.0f, 1.0f, 3.0f, 2.0f}, 1.0});
    REQUIRE(crop.width == 3);
    REQUIRE(crop.height == 2);
    CHECK(Channel(crop, 0, 0, 0) == 2);
    CHECK(Channel(crop, 0, 0, 1) == 1);
    CHECK(Channel(crop, 2, 1, 0) == 4);

    // Half size averages each 2x2 block: pixels 0 and 1 → 0.5, rounded up
    const auto half = CaptureFrame(frame, {8, 4}, {std::nullopt, 0.5});
    REQUIRE(half.width == 4);
    REQUIRE(half.height == 2);
    CHECK(Channel(half, 0, 0, 0) == 1);
    CHECK(Channel(half, 3, 1, 1) == 3);
    CHECK(Channel(half, 0, 0, 3) == 255);

    // A canvas larger than the frame maps onto its pixels
    const auto mapped = CaptureFrame(frame, {16, 8}, {FloatRect{8.0f, 0.0f, 8.0f, 8.0f}, 1.0});
    CHECK(mapped.width == 4);
    CHECK(Channel(mapped, 0, 0, 0) == 4);

    CHECK_THROWS(CaptureFrame(frame, {8, 4}, {FloatRect{6.0f, 0.0f, 4.0f, 2.0f}, 1.0}));
    CHECK_THROWS(CaptureFrame(frame, {8, 4}, {FloatRect{0.0f, 0.0f, 0.0f, 2.0f}, 1.0}));
    CHECK_THROWS(CaptureFrame(frame, {8, 4}, {std::nullopt, 10.0}));
    CHECK_THROWS(CaptureFrame(ViewportPixels{}, {8, 4}, {}));
}

TEST_CASE("Editor.Capture.FolderKeepsTheNewest", "[engine-editor]") {
    TempDirectory directory;
    for (int i = 0; i < 4; ++i) {
        const auto file = NewCapturePath(directory.Path(), "shot");
        CHECK_FALSE(std::filesystem::exists(file));
        std::ofstream(file) << i;
        std::filesystem::last_write_time(file, std::filesystem::file_time_type::clock::now() +
                                                   std::chrono::seconds(i)); // Distinct ages
    }
    std::ofstream(directory.Path() / "notes.txt") << "kept";
    PruneCaptures(directory.Path(), 2);
    std::size_t png = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory.Path()))
        png += entry.path().extension() == ".png";
    CHECK(png == 2);
    CHECK(std::filesystem::exists(directory.Path() / "notes.txt"));
}

TEST_CASE("Editor.Capture.GridPlacesTilesInRowsWithLabels", "[engine-editor]") {
    const std::vector tiles{Gradient(4, 2), Gradient(4, 2), Gradient(4, 2)};
    const auto grid = ComposeFrameGrid(tiles, {"0.00s", "0.50s", "1.00s"}, 2);
    CHECK(grid.image.width == 2 * 4 + 3 * FrameGridGap);
    CHECK(grid.image.height == 2 * 2 + 3 * FrameGridGap);
    REQUIRE(grid.labels.size() == 3);
    CHECK(grid.labels[1].x == FrameGridGap * 2 + 4);
    CHECK(grid.labels[2].y == FrameGridGap * 2 + 2);
    CHECK(grid.labels[2].text == "1.00s");
    // The third tile starts the second row: its pixel (3, 1) is the source's (3, 1)
    CHECK(Channel(grid.image, grid.labels[2].x + 3, grid.labels[2].y + 1, 0) == 3);
    CHECK(Channel(grid.image, grid.labels[2].x + 3, grid.labels[2].y + 1, 1) == 1);
    CHECK_THROWS(ComposeFrameGrid(tiles, {"one"}, 2));
    CHECK_THROWS(ComposeFrameGrid({Gradient(4, 2), Gradient(2, 2)}, {"a", "b"}, 2));
    CHECK_THROWS(ComposeFrameGrid({}, {}, 1));
}

TEST_CASE("Editor.Mark.DragAndClickBecomeCanvasRegions", "[engine-editor]") {
    // A 1600x900 canvas shown at half size from (100, 50)
    const SceneViewport viewport{{100.0f, 50.0f}, {800.0f, 450.0f}, {1600, 900}, {}, {}};
    const auto drag = MarkRegion(viewport, {200.0f, 100.0f}, {300.0f, 150.0f});
    REQUIRE(drag);
    CHECK(*drag == FloatRect{200.0f, 100.0f, 200.0f, 100.0f});
    const auto backwards = MarkRegion(viewport, {300.0f, 150.0f}, {200.0f, 100.0f});
    CHECK(backwards == drag);
    // Dragging past the image stops at the canvas edge
    const auto clamped = MarkRegion(viewport, {800.0f, 400.0f}, {2000.0f, 2000.0f});
    REQUIRE(clamped);
    CHECK(clamped->Right() == 1600.0f);
    CHECK(clamped->Bottom() == 900.0f);
    // A click marks a square around it, kept inside the canvas
    const auto click = MarkRegion(viewport, {102.0f, 52.0f}, {103.0f, 52.0f});
    REQUIRE(click);
    CHECK(*click == FloatRect{0.0f, 0.0f, MarkPointSize, MarkPointSize});
    CHECK_FALSE(MarkRegion(viewport, {50.0f, 50.0f}, {300.0f, 150.0f})); // Pressed outside the image
}

TEST_CASE("Editor.Mark.ContextNamesTheSceneTimeRegionImageAndChangedParameters", "[engine-editor]") {
    SceneDefinition definition;
    definition.id = "drawing.circle";
    auto radius = Parameter(SceneParameterKind::Float);
    radius.id = "radius";
    radius.initial = 10.0;
    auto glow = Parameter(SceneParameterKind::Boolean);
    glow.id = "glow";
    glow.initial = false;
    definition.parameters = {radius, glow};
    const auto line = MarkContext(definition, {{"radius", 24.0}, {"glow", false}}, 1.5, true,
                                  FloatRect{10.0f, 20.0f, 300.0f, 200.0f}, {1600, 900}, "C:/captures/mark.png");
    CHECK(line.starts_with("[editor] scene=drawing.circle t=1.50s(paused) region=10,20 300x200 of 1600x900"));
    CHECK(line.find("image=\"C:/captures/mark.png\"") != std::string::npos);
    CHECK(line.find("radius=24.0") != std::string::npos);
    CHECK(line.find("glow") == std::string::npos); // Unchanged from its initial value
    CHECK(line.ends_with(" "));
    CHECK(line.find('\n') == std::string::npos); // Pasting never submits
    CHECK(MarkContext(definition, {}, 2.0, false, {}, {1600, 900}, "m.png").find("t~2.00s(playing)") !=
          std::string::npos);
}

TEST_CASE("Editor.Mark.WindowRegionsUseFramebufferPixels", "[engine-editor]") {
    // A 1000x600 window at (10, 20) whose framebuffer is twice its UI size
    const auto drag =
        WindowMarkRegion({110.0f, 120.0f}, {60.0f, 70.0f}, {10.0f, 20.0f}, {1000.0f, 600.0f}, {2.0f, 2.0f});
    REQUIRE(drag);
    CHECK(*drag == FloatRect{100.0f, 100.0f, 100.0f, 100.0f});
    const auto click =
        WindowMarkRegion({12.0f, 22.0f}, {12.0f, 22.0f}, {10.0f, 20.0f}, {1000.0f, 600.0f}, {1.0f, 1.0f});
    REQUIRE(click);
    CHECK(*click == FloatRect{0.0f, 0.0f, MarkWindowPointSize, MarkWindowPointSize});
    const auto line =
        WindowMarkContext(FloatRect{5.0f, 6.0f, 70.0f, 80.0f}, {2000, 1200}, "C:/m.png", "drawing.circle");
    CHECK(line == "[editor] window region=5,6 70x80 of 2000x1200 image=\"C:/m.png\" open-scene=drawing.circle ");
    CHECK(WindowMarkContext({}, {2000, 1200}, "m.png", {}).find("open-scene") == std::string::npos);
}
