#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <vector>
#include "Editor/Internals/Pixelator/PixelFrameSampling.h"
#include "Editor/Internals/Pixelator/PixelatorPicture.h"
#include "Editor/Internals/Pixelator/PixelatorPlayback.h"
#include "Editor/Internals/Pixelator/PixelatorView.h"
#include "Editor/Internals/Protocol/PixelatorProtocol.h"

namespace {
// A width x height sprite filled with `ink`, its feet as pivot; part 1 on the left half when `parts`.
PixelSprite Filled(int width, int height, Ink ink, bool parts = false) {
    PixelCanvas canvas(width, height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            if (parts && x < width / 2) {
                const auto part = canvas.Part(1);
                canvas.Set(x, y, ink);
            } else {
                canvas.Set(x, y, ink);
            }
        }
    return PixelSprite(canvas, {width / 2, height});
}

const PixelPalette Palette{Color4{10, 20, 30, 255}, Color4{200, 100, 50, 255}, Color4{0, 255, 0, 128}};

Color4 PixelOf(const ViewportPixels& image, int x, int y) {
    const auto at =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) + static_cast<std::size_t>(x)) * 4;
    return {std::to_integer<std::uint8_t>(image.rgba[at]), std::to_integer<std::uint8_t>(image.rgba[at + 1]),
            std::to_integer<std::uint8_t>(image.rgba[at + 2]), std::to_integer<std::uint8_t>(image.rgba[at + 3])};
}

PixelatorAsset Sample(int width, int height) {
    const auto sprite = Filled(width, height, 2, true);
    auto asset = SamplePixelFrames([&](float) { return GraphicPixels{sprite, Palette}; });
    asset.id = "asset:Graphics/Hero#Idle";
    asset.items = {{"Idle", "asset:Graphics/Hero#Idle"}, {"Run", "asset:Graphics/Hero#Run"}};
    return asset;
}
} // namespace

TEST_CASE("Editor.Pixelator.StillGraphicIsOneFrameOverTheWindow", "[engine-editor]") {
    const auto sprite = Filled(4, 3, 1);
    int calls = 0;
    const auto asset = SamplePixelFrames([&](float) {
        ++calls;
        return GraphicPixels{sprite, Palette};
    });
    CHECK(calls == 240); // 4 s at 60 Hz
    REQUIRE(asset.frames.size() == 1);
    REQUIRE(asset.images.size() == 1);
    REQUIRE(asset.palettes.size() == 1);
    CHECK(asset.frames[0].seconds == Catch::Approx(4.0f).margin(1e-3));
    CHECK(asset.window == Catch::Approx(4.0f));
    CHECK_FALSE(asset.looped);
    CHECK_FALSE(asset.capped);
    CHECK(asset.images[0].width == 4);
    CHECK(asset.images[0].pivot.y == 3);
    CHECK(asset.palettes[0].size() == 4); // Ink 0 and three colours
}

TEST_CASE("Editor.Pixelator.RepeatingAnimationKeepsOneCycle", "[engine-editor]") {
    const auto a = Filled(2, 2, 1);
    const auto b = Filled(2, 2, 2);
    const auto c = Filled(2, 2, 3);
    // a 0.25 s, b 0.25 s, c 0.5 s, repeating every second
    const auto asset = SamplePixelFrames([&](float time) {
        const float phase = time - static_cast<float>(static_cast<int>(time));
        return GraphicPixels{phase < 0.25f ? a : phase < 0.5f ? b : c, Palette};
    });
    REQUIRE(asset.frames.size() == 3);
    CHECK(asset.looped);
    CHECK(asset.images.size() == 3);
    CHECK(asset.frames[0].seconds == Catch::Approx(0.25f).margin(0.02f));
    CHECK(asset.frames[2].seconds == Catch::Approx(0.5f).margin(0.02f));

    // A 3 s cycle of six 0.5 s frames shows only 1.33 passes in the 4 s window: still found.
    std::vector<PixelSprite> six;
    for (int index = 0; index < 6; ++index)
        six.push_back(Filled(2, 2, static_cast<Ink>(index + 1)));
    const auto slow = SamplePixelFrames(
        [&](float time) { return GraphicPixels{six[static_cast<std::size_t>(time * 2.0f) % 6], Palette}; });
    CHECK(slow.looped);
    CHECK(slow.frames.size() == 6);
    // Without a repeat it stays one pass.
    const auto once = SamplePixelFrames([&](float time) {
        return GraphicPixels{six[std::min<std::size_t>(static_cast<std::size_t>(time * 2.0f), 5)], Palette};
    });
    CHECK_FALSE(once.looped);
    CHECK(once.frames.size() == 6);
}

TEST_CASE("Editor.Pixelator.EqualSpritesAndPalettesAreKeptOnce", "[engine-editor]") {
    // A graphic that makes a new sprite (new id) on every call, flashing its palette for the first half second.
    const auto asset = SamplePixelFrames(
        [](float time) {
            return GraphicPixels{Filled(3, 3, 1), time < 0.5f ? Palette.Mixed({255, 255, 255, 255}, 0.8f) : Palette};
        },
        {.window = 1.0f});
    CHECK(asset.images.size() == 1);
    CHECK(asset.palettes.size() == 2);
    REQUIRE(asset.frames.size() == 2);
    CHECK(asset.frames[0].palette != asset.frames[1].palette);
    CHECK_FALSE(asset.looped);
}

TEST_CASE("Editor.Pixelator.SamplingStopsAtItsLimits", "[engine-editor]") {
    int calls = 0;
    const auto everyCall = [&](float) { return GraphicPixels{Filled(1 + calls++ % 50, 2, 1), Palette}; };
    auto asset = SamplePixelFrames(everyCall, {.maxFrames = 8});
    CHECK(asset.capped);
    CHECK(asset.frames.size() == 8);
    CHECK_FALSE(asset.looped);
    calls = 0;
    asset = SamplePixelFrames(everyCall, {.maxBytes = 40}); // 2 + 4 + 6 + 8 + 10 + 12 bytes per sprite pair
    CHECK(asset.capped);
    CHECK(asset.images.size() < 8);
    CHECK_THROWS_AS(SamplePixelFrames([](float) -> GraphicPixels { throw std::runtime_error("broken"); }),
                    std::runtime_error);
}

TEST_CASE("Editor.Pixelator.ViewFitsZoomsAroundThePointerAndMapsPixels", "[engine-editor]") {
    PixelatorView view;
    const Vec2 sprite{112, 92};
    view.Keep({1000, 600}, sprite);
    CHECK(view.Fitted());
    CHECK(view.Zoom() == 6); // 600 / 92 = 6.5
    CHECK(view.Origin().x == Catch::Approx((1000 - 112 * 6) / 2));
    CHECK(view.GridVisible());
    const auto pixel = view.PixelAt(view.ScreenOf({10.5f, 20.5f}), sprite);
    REQUIRE(pixel);
    CHECK(pixel->x == 10);
    CHECK(pixel->y == 20);
    CHECK_FALSE(view.PixelAt(view.ScreenOf({-0.5f, 0.0f}), sprite));
    CHECK_FALSE(view.PixelAt(view.ScreenOf({112.0f, 0.0f}), sprite));

    const Float2 pointer = view.ScreenOf({40.0f, 30.0f});
    view.ZoomAt(pointer, 1);
    CHECK(view.Zoom() == 8);
    CHECK_FALSE(view.Fitted());
    CHECK(view.ScreenOf({40.0f, 30.0f}).x == Catch::Approx(pointer.x).margin(1.0f));
    CHECK(view.ScreenOf({40.0f, 30.0f}).y == Catch::Approx(pointer.y).margin(1.0f));
    view.Keep({800, 600}, sprite); // Touched: no refit
    CHECK(view.Zoom() == 8);
    view.Pan({10.4f, -3.0f});
    CHECK(view.Origin().x == std::round(view.Origin().x));

    CHECK(PixelatorView::StepZoom(1, 1) == 2);
    CHECK(PixelatorView::StepZoom(5, 1) == 6);
    CHECK(PixelatorView::StepZoom(5, -1) == 4);
    CHECK(PixelatorView::StepZoom(64, 1) == 64);
    CHECK(PixelatorView::StepZoom(1, -1) == 1);
    CHECK(PixelatorView::StepZoom(8, 3) == 24);

    view.Actual({800, 600}, sprite);
    CHECK(view.Zoom() == 1);
    CHECK_FALSE(view.GridVisible());
    view.Fit({50, 50}, {640, 360}); // Larger than the area: still whole pixels
    CHECK(view.Zoom() == 1);
    view.Fit({4000, 4000}, {2, 2});
    CHECK(view.Zoom() == PixelatorView::MaxZoom);
}

TEST_CASE("Editor.Pixelator.ColorsCountsAndIsolation", "[engine-editor]") {
    const auto image = PixelatorImageOf(Filled(4, 2, 2, true));
    const std::vector<Color4> palette(Palette.Colors().begin(), Palette.Colors().end());
    auto colors = PixelatorColors(image, palette, PixelatorViewMode::Color);
    CHECK(colors[0] == palette[2]);
    colors = PixelatorColors(image, palette, PixelatorViewMode::Inks);
    CHECK(colors[0] == PixelatorIndexColor(2));
    colors = PixelatorColors(image, palette, PixelatorViewMode::Parts);
    CHECK(colors[0] == PixelatorIndexColor(2)); // Part 1
    CHECK(colors[3] == PixelatorIndexColor(1)); // No part
    CHECK(PixelatorIndexColor(1) != PixelatorIndexColor(2));
    colors = PixelatorColors(image, palette, PixelatorViewMode::Color, Ink{1});
    CHECK(colors[0].a == palette[2].a * PixelatorDimAlpha / 255);
    CHECK(PixelatorColors(image, {}, PixelatorViewMode::Color)[0].a == 0); // A palette without the ink

    CHECK(PixelatorInkCounts(image) == std::vector<PixelatorCount>{{2, 8}});
    CHECK(PixelatorPartCounts(image) == std::vector<PixelatorCount>{{0, 4}, {1, 4}});
}

TEST_CASE("Editor.Pixelator.GridsCheckerAndPictureZoom", "[engine-editor]") {
    CHECK(PixelatorTileLines(40, 16) == std::vector<int>{16, 32});
    CHECK(PixelatorTileLines(32, 16) == std::vector<int>{16});
    CHECK(PixelatorTileLines(32, 0).empty());
    CHECK(PixelatorCheckerSize(1) == 16);
    CHECK(PixelatorCheckerSize(3) == 18);
    CHECK(PixelatorCheckerSize(16) == 16);
    CHECK(PixelatorCheckerSize(24) == 24);
    CHECK(PixelatorPictureZoom({112, 92}, 0) == 9);
    CHECK(PixelatorPictureZoom({640, 360}, 0) == 1);
    CHECK(PixelatorPictureZoom({640, 360}, 8) == 8);
    CHECK(PixelatorPictureZoom({640, 360}, 64) == 12); // 640 x 12 + margins fits 8192
    CHECK(PixelatorPictureZoom({4, 4}, 0) == 64);
}

TEST_CASE("Editor.Pixelator.PictureShowsPixelsGridAndPivot", "[engine-editor]") {
    PixelCanvas canvas(3, 2);
    canvas.Set(0, 0, 2);
    canvas.Set(2, 1, 1);
    const auto image = PixelatorImageOf(PixelSprite(canvas, {1, 2}));
    const std::vector<Color4> palette(Palette.Colors().begin(), Palette.Colors().end());
    PixelatorPictureOptions options;
    options.zoom = 10;
    options.background = PixelatorBackground::Dark;
    const auto picture = ComposePixelatorPicture(image, palette, options);
    CHECK(picture.width == 3 * 10 + 16);
    CHECK(picture.height == 2 * 10 + 16);
    CHECK(PixelOf(picture, 8 + 5, 8 + 5) == palette[2]);
    CHECK(PixelOf(picture, 8 + 25, 8 + 15) == palette[1]);
    CHECK(PixelOf(picture, 8 + 15, 8 + 5) == PixelatorDarkBackground); // Clear pixel
    const Color4 line = PixelOf(picture, 8 + 20, 8 + 15);              // The grid on pixel (2, 1)'s left edge
    CHECK(line.g < palette[1].g);
    CHECK(PixelOf(picture, 8 + 10, 8 + 20 + 3) == PixelatorPivotColor); // Pivot arm below the sprite

    options.grid = false;
    options.pivot = false;
    const auto plain = ComposePixelatorPicture(image, palette, options);
    CHECK(PixelOf(plain, 8 + 20, 8 + 15) == palette[1]);
    CHECK(PixelOf(plain, 8 + 10, 8 + 20 + 3) != PixelatorPivotColor);
    options.zoom = 0;
    CHECK_THROWS(ComposePixelatorPicture(image, palette, options));
    options.zoom = 2;
    CHECK_THROWS(ComposePixelatorPicture(PixelatorImage{}, palette, options));
}

TEST_CASE("Editor.Pixelator.MessageRoundTripsAndRejectsBrokenData", "[engine-editor]") {
    for (const int width : {1, 2, 3, 7}) {
        const auto asset = Sample(width, 3);
        CHECK(DecodePixelatorAsset(EncodePixelatorAsset(asset)) == asset);
    }
    auto failed = PixelatorAsset{.id = "asset:Graphics/Rock", .error = "Sampling the pixels failed: no"};
    CHECK(DecodePixelatorAsset(EncodePixelatorAsset(failed)) == failed);

    auto json = EncodePixelatorAsset(Sample(4, 4));
    auto broken = json;
    broken["images"][0]["size"] = {5, 4};
    CHECK_THROWS(DecodePixelatorAsset(broken));
    broken = json;
    broken["frames"][0]["image"] = 9;
    CHECK_THROWS(DecodePixelatorAsset(broken));
    broken = json;
    broken["images"][0]["inks"] = "!!!!";
    CHECK_THROWS(DecodePixelatorAsset(broken));
    broken = json;
    broken["item"] = 5;
    CHECK_THROWS(DecodePixelatorAsset(broken));
    broken = json;
    broken["palettes"][0][0] = "#123";
    CHECK_THROWS(DecodePixelatorAsset(broken));
}

TEST_CASE("Editor.Pixelator.ControlRequestAndSummary", "[engine-editor]") {
    auto request = DecodePixelatorRequest(nlohmann::json::object());
    CHECK(request.picture.view == PixelatorViewMode::Color);
    CHECK(request.picture.zoom == 0);
    CHECK_FALSE(request.picture.grid);
    CHECK(request.frame == 0);
    request = DecodePixelatorRequest({{"view", "parts"},
                                      {"zoom", 8},
                                      {"frame", 2},
                                      {"grid", false},
                                      {"tiles", 16},
                                      {"background", "light"},
                                      {"pivot", false},
                                      {"ink", 3}});
    CHECK(request.picture.view == PixelatorViewMode::Parts);
    CHECK(request.picture.zoom == 8);
    CHECK(request.frame == 2);
    CHECK(request.picture.grid == false);
    CHECK(request.picture.tiles == 16);
    CHECK(request.picture.background == PixelatorBackground::Light);
    CHECK_FALSE(request.picture.pivot);
    CHECK(request.picture.focus == Ink{3});
    CHECK_THROWS(DecodePixelatorRequest({{"view", "pixels"}}));
    CHECK_THROWS(DecodePixelatorRequest({{"zoom", 65}}));
    CHECK_THROWS(DecodePixelatorRequest({{"zoom", 2.5}}));
    CHECK_THROWS(DecodePixelatorRequest({{"ink", 0}}));
    CHECK_THROWS(DecodePixelatorRequest({{"grid", "yes"}}));
    CHECK(std::string(PixelatorViewName(PixelatorViewMode::Inks)) == "inks");

    const auto summary = PixelatorSummary(Sample(4, 2), 0);
    CHECK(summary.at("asset") == "asset:Graphics/Hero#Idle");
    CHECK(summary.at("item") == "Idle");
    CHECK(summary.at("items").size() == 2);
    CHECK(summary.at("size") == nlohmann::json::array({4, 2}));
    CHECK(summary.at("pivot") == nlohmann::json::array({2, 2}));
    CHECK(summary.at("frames") == 1);
    REQUIRE(summary.at("inks").size() == 1);
    CHECK(summary.at("inks")[0].at("ink") == 2);
    CHECK(summary.at("inks")[0].at("color") == "#c86432");
    CHECK(summary.at("inks")[0].at("pixels") == 8);
    CHECK(summary.at("parts").size() == 2);
    CHECK(PixelatorHex({0, 255, 0, 128}) == "#00ff0080");
}

TEST_CASE("Editor.Pixelator.PlaybackLoopsAndSteps", "[engine-editor]") {
    const std::vector<PixelatorFrame> frames{{0, 0, 0.1f}, {1, 0, 0.2f}, {2, 0, 0.1f}};
    PixelatorPlayback playback;
    playback.Reset(frames);
    CHECK(playback.Playing());
    playback.Advance(frames, 0.05);
    CHECK(playback.Frame() == 0);
    playback.Advance(frames, 0.1);
    CHECK(playback.Frame() == 1);
    playback.Advance(frames, 0.3); // 0.05 into frame 1 + 0.3: through frame 2, back to 0
    CHECK(playback.Frame() == 0);
    playback.Step(frames, -1);
    CHECK_FALSE(playback.Playing());
    CHECK(playback.Frame() == 2);
    playback.Step(frames, 2);
    CHECK(playback.Frame() == 1);
    playback.Advance(frames, 5.0); // Paused
    CHECK(playback.Frame() == 1);
    playback.Select(frames, 9);
    CHECK(playback.Frame() == 2);
    const std::vector<PixelatorFrame> one{{0, 0, 4.0f}};
    playback.Reset(one);
    CHECK_FALSE(playback.Playing());
}
