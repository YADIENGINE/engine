#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include "Editor/Internals/Protocol/EditorControlProtocol.h"
#include "Editor/Internals/Session/FrameCapture.h"
#include "Tests/Support/TempDirectory.h"

namespace {
PreviewParameter Parameter(PreviewParameterKind kind) {
    PreviewParameter parameter;
    parameter.id = "value";
    parameter.kind = kind;
    return parameter;
}
// A frame whose pixel (x, y) is (x, y, 0, 255).
PreviewPixels Gradient(int width, int height) {
    PreviewPixels frame;
    frame.width = width;
    frame.height = height;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            for (const int channel : {x, y, 0, 255})
                frame.rgba.push_back(static_cast<std::byte>(channel));
    return frame;
}
int Channel(const PreviewPixels& frame, int x, int y, int channel) {
    return std::to_integer<int>(frame.rgba[static_cast<std::size_t>((y * frame.width + x) * 4 + channel)]);
}
} // namespace

TEST_CASE("Editor.Control.ValuesFollowTheParameterKind", "[engine-editor]") {
    CHECK(ControlValueOf(Parameter(PreviewParameterKind::Integer), 3) == PreviewValue{std::int64_t{3}});
    CHECK(ControlValueOf(Parameter(PreviewParameterKind::Integer), 4.0) == PreviewValue{std::int64_t{4}});
    CHECK_THROWS(ControlValueOf(Parameter(PreviewParameterKind::Integer), 4.5));
    CHECK(ControlValueOf(Parameter(PreviewParameterKind::Float), 2) == PreviewValue{2.0});
    CHECK_THROWS(ControlValueOf(Parameter(PreviewParameterKind::Float), "2"));
    CHECK(ControlValueOf(Parameter(PreviewParameterKind::Boolean), true) == PreviewValue{true});
    CHECK_THROWS(ControlValueOf(Parameter(PreviewParameterKind::Boolean), 1));
    CHECK(ControlValueOf(Parameter(PreviewParameterKind::Choice), "Fast") == PreviewValue{std::string("Fast")});
    CHECK(ControlValueOf(Parameter(PreviewParameterKind::Color), nlohmann::json::array({1, 0, 0})) ==
          PreviewValue{PreviewColor{1.0f, 0.0f, 0.0f, 1.0f}});
    CHECK(ControlValueOf(Parameter(PreviewParameterKind::Color), "#ff000080") ==
          PreviewValue{PreviewColor{1.0f, 0.0f, 0.0f, 128.0f / 255.0f}});
    CHECK_THROWS(ControlValueOf(Parameter(PreviewParameterKind::Color), "red"));
    CHECK_THROWS(ControlValueOf(Parameter(PreviewParameterKind::Color), nlohmann::json::array({1, 0})));
    // Values go back out as plain JSON
    CHECK(ControlValue(PreviewColor{0.0f, 1.0f, 0.0f, 1.0f}) == nlohmann::json::array({0.0f, 1.0f, 0.0f, 1.0f}));
    CHECK(ControlValue(std::int64_t{7}) == 7);
}

TEST_CASE("Editor.Control.CatalogEntryShowsCurrentValuesAndActions", "[engine-editor]") {
    PreviewDefinition definition;
    definition.id = "drawing.circle";
    definition.name = "Circle";
    auto radius = Parameter(PreviewParameterKind::Float);
    radius.id = "radius";
    radius.initial = 10.0;
    radius.maximum = 50.0;
    auto mode = Parameter(PreviewParameterKind::Choice);
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
    CHECK_THROWS(CaptureFrame(PreviewPixels{}, {8, 4}, {}));
}

TEST_CASE("Editor.Capture.FolderKeepsTheNewest", "[engine-editor]") {
    TempDirectory directory;
    for (int i = 0; i < 4; ++i) {
        const auto file = NewCapturePath(directory.Path());
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
