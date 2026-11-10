#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <vector>
#include "Editor/Internals/Pixelator/PixelatorFrames.h"
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
    auto asset = PixelatorFramesOf(GraphicPixels::Still(Filled(width, height, 2, true), Palette));
    asset.id = "asset:Graphics/Hero#Idle";
    asset.items = {{"Idle", "asset:Graphics/Hero#Idle"}, {"Run", "asset:Graphics/Hero#Run"}};
    return asset;
}

PixelSprite MakeFrame(std::size_t index) { return Filled(2, 2, static_cast<Ink>(index + 1)); }
} // namespace

TEST_CASE("Editor.Pixelator.StillGraphicIsOneFrame", "[engine-editor]") {
    const auto asset = PixelatorFramesOf(GraphicPixels::Still(Filled(4, 3, 1), Palette));
    REQUIRE(asset.frames.size() == 1);
    REQUIRE(asset.images.size() == 1);
    REQUIRE(asset.palettes.size() == 1);
    CHECK(asset.frames[0].seconds == Catch::Approx(1.0f));
    CHECK(asset.loops);
    CHECK_FALSE(asset.capped);
    CHECK(asset.images[0].width == 4);
    CHECK(asset.images[0].pivot.y == 3);
    CHECK(asset.palettes[0].size() == 4); // Ink 0 and three colours
    CHECK(asset.inkNames.empty());
}

TEST_CASE("Editor.Pixelator.DeclaredFramesKeepTheirDurationsAndOrder", "[engine-editor]") {
    // The game's own frames: a set played by an animation, exactly as the game draws them.
    const PixelSpriteSet sprites(3, &MakeFrame);
    const PixelAnimation walk{{{0, 0.25f}, {1, 0.125f}, {2, 0.5f}, {1, 0.125f}}};
    auto pixels = GraphicPixels::Animated(sprites, walk, Palette);
    pixels.inkNames = {"Line", "Skin"};
    pixels.partNames = {"Body"};
    pixels.ramps = {{1, 1, 2, 3, 3}};
    const auto asset = PixelatorFramesOf(pixels);
    REQUIRE(asset.frames.size() == 4);
    CHECK(asset.images.size() == 3); // Sprite 1 shows twice, sent once
    CHECK(asset.frames[0].seconds == 0.25f);
    CHECK(asset.frames[1].seconds == 0.125f);
    CHECK(asset.frames[3].image == asset.frames[1].image);
    CHECK(asset.loops);
    // Frame by frame the editor shows what the game draws at that time.
    float time = 0.0f;
    for (std::size_t frame = 0; frame < asset.frames.size(); ++frame) {
        CHECK(asset.images[asset.frames[frame].image] ==
              PixelatorImageOf(sprites[walk.SpriteAt(time + asset.frames[frame].seconds * 0.5f)]));
        time += asset.frames[frame].seconds;
    }
    CHECK(asset.InkName(1) == "Line");
    CHECK(asset.InkName(2) == "Skin");
    CHECK(asset.InkName(3).empty());
    CHECK(asset.PartName(1) == "Body");
    CHECK(asset.PartName(0).empty());
    CHECK(PixelatorInkLabel(asset, 2) == "2 Skin");
    CHECK(PixelatorInkLabel(asset, 3) == "3");
    REQUIRE(asset.ramps.size() == 1);
    CHECK(asset.ramps[0].base == 2);

    const PixelAnimation once({{0, 0.1f}, {2, 0.1f}}, false);
    CHECK_FALSE(PixelatorFramesOf(GraphicPixels::Animated(sprites, once, Palette)).loops);
}

TEST_CASE("Editor.Pixelator.EqualSpritesAreKeptOnceAndSizeIsCapped", "[engine-editor]") {
    GraphicPixels pixels;
    pixels.palette = Palette;
    for (int index = 0; index < 4; ++index) // A new sprite (new id) with the same pixels each time
        pixels.frames.push_back({Filled(3, 3, 1), 0.1f});
    auto asset = PixelatorFramesOf(pixels);
    CHECK(asset.images.size() == 1);
    CHECK(asset.frames.size() == 4);

    pixels.frames.clear();
    for (int width = 1; width <= 8; ++width)
        pixels.frames.push_back({Filled(width, 2, 1), 0.1f});
    asset = PixelatorFramesOf(pixels, 40); // 4 + 8 + 12 + 16 bytes fit
    CHECK(asset.capped);
    CHECK(asset.images.size() == 4);
    CHECK(asset.frames.size() == 4);

    pixels.frames = {{Filled(2, 2, 1), 0.0f}};
    CHECK_THROWS_AS(PixelatorFramesOf(pixels), std::invalid_argument);
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
    auto failed = PixelatorAsset{.id = "asset:Graphics/Rock", .error = "Reading the pixels failed: no"};
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
