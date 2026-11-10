#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include "Editor/Internals/Pixelator/PixelatorDiff.h"
#include "Editor/Internals/Pixelator/PixelatorExport.h"
#include "Editor/Internals/Pixelator/PixelatorFrames.h"
#include "Editor/Internals/Pixelator/PixelatorLayout.h"
#include "Editor/Internals/Pixelator/PixelatorPicture.h"
#include "Editor/Internals/Protocol/PixelatorProtocol.h"

// Pixelator V2 (D65) library logic: onion skin, comparison panes, the tile preview, the changes since the last
// build, export, names and ramps. The panel and `ctl` do not use most of it yet (PIXELATOR.md).
namespace {
const PixelPalette Palette{Color4{10, 20, 30, 255}, Color4{200, 100, 50, 255}, Color4{0, 255, 0, 255}};

// A sprite of `width` x `height` with one pixel of ink 1 at (x, y) and ink 2 elsewhere on row 0; pivot `pivot`.
PixelSprite Dot(int width, int height, int x, int y, Vec2 pivot) {
    PixelCanvas canvas(width, height);
    canvas.Set(x, y, 1);
    return PixelSprite(canvas, pivot);
}

PixelatorAsset AssetOf(std::vector<GraphicPixels::Frame> frames, bool loops = true, PixelPalette palette = Palette) {
    GraphicPixels pixels;
    pixels.frames = std::move(frames);
    pixels.palette = std::move(palette);
    pixels.loops = loops;
    auto asset = PixelatorFramesOf(pixels);
    asset.id = "asset:Graphics/Characters/Hero#Walk";
    asset.items = {{"Idle", "asset:Graphics/Characters/Hero#Idle"}, {"Walk", "asset:Graphics/Characters/Hero#Walk"}};
    asset.item = 1;
    return asset;
}

Color4 PixelOf(const ViewportPixels& image, int x, int y) {
    const auto at =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) + static_cast<std::size_t>(x)) * 4;
    return {std::to_integer<std::uint8_t>(image.rgba[at]), std::to_integer<std::uint8_t>(image.rgba[at + 1]),
            std::to_integer<std::uint8_t>(image.rgba[at + 2]), std::to_integer<std::uint8_t>(image.rgba[at + 3])};
}

// The frames of a GIF and their delays, read from its blocks.
struct GifFacts {
    int width = 0;
    int height = 0;
    std::vector<int> delays;
    int images = 0;
    bool loops = false;
};
GifFacts ReadGif(const std::vector<std::byte>& bytes) {
    const auto byte = [&](std::size_t at) {
        if (at >= bytes.size()) throw std::runtime_error("GIF ends early");
        return std::to_integer<int>(bytes[at]);
    };
    GifFacts facts;
    if (bytes.size() < 13 || byte(0) != 'G' || byte(1) != 'I' || byte(2) != 'F') throw std::runtime_error("No GIF");
    facts.width = byte(6) | byte(7) << 8;
    facts.height = byte(8) | byte(9) << 8;
    std::size_t at = 13;
    if (byte(10) & 0x80) at += 3u << ((byte(10) & 7) + 1);
    const auto skipBlocks = [&] {
        while (const int size = byte(at)) at += static_cast<std::size_t>(size) + 1;
        ++at;
    };
    while (true) {
        const int kind = byte(at++);
        if (kind == 0x3B) break;
        if (kind == 0x21) {
            const int label = byte(at++);
            if (label == 0xF9) facts.delays.push_back(byte(at + 2) | byte(at + 3) << 8);
            if (label == 0xFF && byte(at) == 11 && byte(at + 1) == 'N') facts.loops = true;
            skipBlocks();
        } else if (kind == 0x2C) {
            const int packed = byte(at + 8);
            at += 9;
            if (packed & 0x80) at += 3u << ((packed & 7) + 1);
            ++at; // LZW minimum code size
            skipBlocks();
            ++facts.images;
        } else {
            throw std::runtime_error("Unknown GIF block");
        }
    }
    return facts;
}
} // namespace

TEST_CASE("Editor.Pixelator.OnionSkinPicksNeighboursAndFades", "[engine-editor]") {
    PixelatorOnionOptions options; // 1 each way
    auto ghosts = PixelatorOnionGhosts(5, 2, true, options);
    REQUIRE(ghosts.size() == 2);
    // Farthest first; with one each way both are 1 away: the earlier one is added first, so drawn last.
    CHECK(ghosts[1].frame == 1);
    CHECK(ghosts[1].distance == -1);
    CHECK(ghosts[1].tint == PixelatorOnionBefore);
    CHECK(ghosts[0].frame == 3);
    CHECK(ghosts[0].tint == PixelatorOnionAfter);
    CHECK(ghosts[0].opacity == Catch::Approx(options.opacity));

    options.before = 3;
    options.after = 2;
    ghosts = PixelatorOnionGhosts(8, 0, true, options); // Wraps around the start of a loop
    REQUIRE(ghosts.size() == 5);
    CHECK(ghosts.front().distance == -3);
    CHECK(ghosts.front().frame == 5);
    CHECK(ghosts.front().opacity == Catch::Approx(options.opacity / 3.0f));
    CHECK(ghosts.back().distance == -1);
    CHECK(ghosts.back().frame == 7);

    ghosts = PixelatorOnionGhosts(8, 0, false, options); // A one-shot animation stops at its ends
    REQUIRE(ghosts.size() == 2);
    CHECK(ghosts[0].frame == 2);
    CHECK(ghosts[1].frame == 1);

    options = {.before = 3, .after = 3};
    ghosts = PixelatorOnionGhosts(3, 1, true, options); // A short loop shows each frame once
    CHECK(ghosts.size() == 2);
    CHECK(PixelatorOnionGhosts(1, 0, true, options).empty());
    options.tinted = false;
    CHECK_FALSE(PixelatorOnionGhosts(4, 0, true, options)[0].tint);

    const auto image = PixelatorImageOf(Dot(2, 1, 0, 0, {}));
    const std::vector<Color4> palette(Palette.Colors().begin(), Palette.Colors().end());
    const auto colors = PixelatorGhostColors(image, palette, {0, -1, 0.5f, PixelatorOnionBefore});
    CHECK(colors[0] == Color4{133, 46, 51, 128}); // Halfway to red, at half opacity
    CHECK(colors[1].a == 0);
    CHECK(PixelatorGhostColors(image, palette, {0, 1, 1.0f, std::nullopt})[0] == palette[1]);
}

TEST_CASE("Editor.Pixelator.FramesAlignOnTheirPivots", "[engine-editor]") {
    // A 4 x 4 sprite standing on (2, 4) and a 6 x 2 one on (1, 2): the box spans both.
    const auto asset = AssetOf({{Dot(4, 4, 0, 0, {2, 4}), 0.1f}, {Dot(6, 2, 0, 0, {1, 2}), 0.1f}});
    const auto box = PixelatorFramesBox(asset);
    CHECK(box.size == Vec2{7, 4});
    CHECK(box.pivot == Vec2{2, 4});
    CHECK(PixelatorPivotOffset(asset.images[0], asset.images[1]) == Vec2{1, 2});
    CHECK(PixelatorFramesBox(PixelatorAsset{}).size == Vec2{});
}

TEST_CASE("Editor.Pixelator.ComparisonPanesAndTilePreview", "[engine-editor]") {
    auto panes = PixelatorPanes({1000, 600}, 2, 6);
    REQUIRE(panes.size() == 2);
    CHECK(panes[0].size == Float2{497, 600});
    CHECK(panes[1].at == Float2{503, 0});
    panes = PixelatorPanes({400, 900}, 3, 6); // Taller than wide: stacked
    REQUIRE(panes.size() == 3);
    CHECK(panes[2].at.x == 0);
    CHECK(panes[2].at.y == 2 * (296 + 6));
    panes = PixelatorPanes({800, 600}, 4, 0);
    REQUIRE(panes.size() == 4);
    CHECK(panes[3].at == Float2{400, 300});
    CHECK(PixelatorPanes({800, 600}, 9, 0).size() == PixelatorMaxPanes);

    CHECK(PixelatorTiledPixel({17, 3}, {16, 16}) == Vec2{1, 3});
    CHECK(PixelatorTiledPixel({-1, 32}, {16, 16}) == Vec2{15, 0});

    // The tile preview repeats the frame 3 x 3: its seams sit side by side.
    const auto image = PixelatorImageOf(Dot(2, 2, 1, 0, {}));
    const std::vector<Color4> palette(Palette.Colors().begin(), Palette.Colors().end());
    PixelatorPictureOptions options;
    options.zoom = 1;
    options.grid = false;
    options.pivot = false;
    options.background = PixelatorBackground::Dark;
    PixelatorPictureExtras extras;
    extras.repeat = PixelatorTileRepeat;
    const auto picture = ComposePixelatorPicture(image, palette, options, extras);
    CHECK(picture.width == 6 + 16);
    CHECK(PixelOf(picture, 8 + 1, 8) == palette[1]);
    CHECK(PixelOf(picture, 8 + 2, 8) == PixelatorDarkBackground);
    CHECK(PixelOf(picture, 8 + 3, 8) == palette[1]);
    CHECK(PixelOf(picture, 8 + 5, 8 + 4) == palette[1]);
}

TEST_CASE("Editor.Pixelator.PictureLayersGhostsAndMarks", "[engine-editor]") {
    const auto current = PixelatorImageOf(Dot(3, 1, 2, 0, {}));
    const auto ghost = PixelatorImageOf(Dot(3, 1, 0, 0, {}));
    const std::vector<Color4> palette(Palette.Colors().begin(), Palette.Colors().end());
    PixelatorPictureOptions options;
    options.zoom = 4;
    options.grid = false;
    options.pivot = false;
    options.background = PixelatorBackground::Dark;
    PixelatorPictureExtras extras;
    extras.under.push_back({&ghost, PixelatorGhostColors(ghost, palette, {0, -1, 1.0f, std::nullopt}), {1, 0}});
    extras.marks = {{2, 0}};
    const auto picture = ComposePixelatorPicture(current, palette, options, extras);
    CHECK(PixelOf(picture, 8 + 4 + 2, 8 + 2) == palette[1]);         // The ghost, moved one pixel right
    CHECK(PixelOf(picture, 8 + 8 + 2, 8 + 2) == palette[1]);         // The frame itself
    CHECK(PixelOf(picture, 8 + 8 - 1, 8 + 2) == PixelatorMarkColor); // The ring around the marked pixel
    CHECK(PixelOf(picture, 8 + 2, 8 + 2) == PixelatorDarkBackground);
}

TEST_CASE("Editor.Pixelator.DiffFindsChangedPixelsPaletteAndFrames", "[engine-editor]") {
    const auto before = AssetOf({{Dot(4, 4, 1, 1, {2, 4}), 0.1f}, {Dot(4, 4, 2, 2, {2, 4}), 0.1f}});
    CHECK(DiffPixelator(before, before).Same());

    // One pixel moved in frame 0, a colour changed, a frame added.
    PixelPalette brighter = Palette;
    brighter.Set(2, Color4{40, 40, 40, 255});
    const auto after = AssetOf({{Dot(4, 4, 1, 2, {2, 4}), 0.1f}, {Dot(4, 4, 2, 2, {2, 4}), 0.1f},
                                {Dot(4, 4, 0, 0, {2, 4}), 0.2f}},
                               true, brighter);
    const auto diff = DiffPixelator(before, after);
    CHECK_FALSE(diff.Same());
    REQUIRE(diff.palette.size() == 1);
    CHECK(diff.palette[0].ink == 2);
    CHECK(diff.palette[0].after == Color4{40, 40, 40, 255});
    REQUIRE(diff.frames.size() == 2);
    CHECK(diff.frames[0].frame == 0);
    CHECK(diff.frames[0].pixels == std::vector<Vec2>{{1, 1}, {1, 2}});
    CHECK(diff.Frame(1) == nullptr);
    CHECK(diff.frames[1].added);
    CHECK(diff.ChangedPixels() == 3);

    // A sprite that grew on its left, aligned on its pivot: the old pixels did not move.
    const auto wider = AssetOf({{Dot(6, 4, 3, 1, {4, 4}), 0.1f}, {Dot(4, 4, 2, 2, {2, 4}), 0.1f}});
    const auto grown = DiffPixelator(before, wider);
    REQUIRE(grown.frames.size() == 1);
    CHECK(grown.frames[0].pixels.empty());
    CHECK(grown.frames[0].sizeAfter == Vec2{6, 4});

    const std::vector<Color4> palette(Palette.Colors().begin(), Palette.Colors().end());
    const auto colors = PixelatorDiffColors(after.images[0], palette, diff.Frame(0));
    CHECK(colors[1 * 4 + 1] == PixelatorDiffRemoved); // Became clear
    CHECK(colors[2 * 4 + 1] == palette[1]);           // Became ink 1
    CHECK(PixelatorDiffColors(after.images[1], palette, nullptr)[2 * 4 + 2].a == palette[1].a * PixelatorDiffDimAlpha / 255);

    const auto json = EncodePixelatorDiff(diff);
    CHECK(json.at("changedPixels") == 3);
    CHECK(json.at("palette")[0].at("before") == "#c86432");
    CHECK(json.at("frames")[1].at("added") == true);
}

TEST_CASE("Editor.Pixelator.ExportComposesFramesSheetsAndNames", "[engine-editor]") {
    const auto asset = AssetOf({{Dot(2, 2, 0, 0, {1, 2}), 0.1f}, {Dot(3, 1, 2, 0, {1, 1}), 0.2f},
                                {Dot(2, 2, 1, 1, {1, 2}), 0.1f}});
    // Box: left 1, right 2, top 2: 3 x 2 with the pivot at (1, 2).
    const auto frame = ComposePixelatorFrame(asset, 0, 2);
    CHECK(frame.width == 6);
    CHECK(frame.height == 4);
    CHECK(PixelOf(frame, 0, 0) == Color4{10, 20, 30, 255}); // Sprite pixel (0, 0) at 2x
    CHECK(PixelOf(frame, 1, 1) == Color4{10, 20, 30, 255});
    CHECK(PixelOf(frame, 2, 0).a == 0); // Transparent around it
    const auto second = ComposePixelatorFrame(asset, 1, 1);
    CHECK(PixelOf(second, 2, 1) == Color4{10, 20, 30, 255}); // Pivot-aligned: its row is the box's last

    const auto strip = ComposePixelatorSheet(asset, 3, 1);
    CHECK(strip.width == 9);
    CHECK(strip.height == 2);
    CHECK(PixelatorSheetColumns(3, 0) == 2);
    CHECK(PixelatorSheetColumns(10, 0) == 4);
    CHECK(PixelatorSheetColumns(3, 8) == 3);
    const auto grid = ComposePixelatorSheet(asset, 0, 1);
    CHECK(grid.width == 6);
    CHECK(grid.height == 4);
    CHECK(PixelOf(grid, 1, 2 + 1) == Color4{10, 20, 30, 255}); // Frame 2 in the second row
    CHECK_THROWS(ComposePixelatorFrame(asset, 3, 1));
    CHECK_THROWS(ComposePixelatorSheet(asset, 0, 9));

    CHECK(PixelatorExportFolderName(asset) == "Hero");
    CHECK(PixelatorExportFileName(asset, {PixelatorExportKind::Frame, 1, 2, 0}) == "Walk-frame3.png");
    CHECK(PixelatorExportFileName(asset, {PixelatorExportKind::Strip}) == "Walk-strip.png");
    CHECK(PixelatorExportFileName(asset, {PixelatorExportKind::Gif}) == "Walk.gif");
    auto single = asset;
    single.items.clear();
    single.id = "asset:Engine/Backgrounds/Sky";
    CHECK(PixelatorExportFileName(single, {PixelatorExportKind::Grid}) == "Sky-grid.png");
    CHECK(PixelatorExportFolder("C:/Game", asset) == std::filesystem::path("C:/Game/Exports/Hero"));
    CHECK(PixelatorExportKindOf("gif") == PixelatorExportKind::Gif);
    CHECK_THROWS(PixelatorExportKindOf("webp"));

    const auto folder = std::filesystem::temp_directory_path() / "PixelatorExportTest";
    std::filesystem::remove_all(folder);
    int encoded = 0;
    const auto file = WritePixelatorExport(folder, asset, {PixelatorExportKind::Strip, 2, 0, 0},
                                           [&](const ViewportPixels& picture) {
                                               ++encoded;
                                               CHECK(picture.width == 18);
                                               return std::vector<std::byte>(3, std::byte{7});
                                           });
    CHECK(encoded == 1);
    CHECK(file == folder / "Exports" / "Hero" / "Walk-strip.png");
    CHECK(std::filesystem::file_size(file) == 3);
    std::filesystem::remove_all(folder);
    CHECK_THROWS(WritePixelatorExport({}, asset, {}, [](const ViewportPixels&) { return std::vector<std::byte>{}; }));
}

TEST_CASE("Editor.Pixelator.GifKeepsFramesAndTiming", "[engine-editor]") {
    // 0.15 s, 0.1 s, 0.015 s: hundredths that add up to the animation's 0.265 s, none below 2.
    const auto asset = AssetOf({{Dot(4, 4, 0, 0, {2, 4}), 0.15f}, {Dot(4, 4, 1, 1, {2, 4}), 0.1f},
                                {Dot(4, 4, 2, 2, {2, 4}), 0.015f}});
    CHECK(PixelatorGifDelays(asset) == std::vector<int>{15, 10, 2});
    const auto thirds = AssetOf({{Dot(2, 2, 0, 0, {}), 1.0f / 30.0f}, {Dot(2, 2, 1, 1, {}), 1.0f / 30.0f},
                                 {Dot(2, 2, 0, 1, {}), 1.0f / 30.0f}});
    CHECK(PixelatorGifDelays(thirds) == std::vector<int>{3, 4, 3}); // 10 hundredths in all

    const auto gif = EncodePixelatorGif(asset, 3);
    const auto facts = ReadGif(gif);
    CHECK(facts.width == 12);
    CHECK(facts.height == 12);
    CHECK(facts.images == 3);
    CHECK(facts.delays == std::vector<int>{15, 10, 2});
    CHECK(facts.loops);
    CHECK_THROWS(EncodePixelatorGif(asset, 0));
}

TEST_CASE("Editor.Pixelator.NamesAndRampsTravelWithTheGraphic", "[engine-editor]") {
    GraphicPixels pixels = GraphicPixels::Still(Dot(2, 2, 0, 0, {}), Palette);
    pixels.inkNames = {"Line", "Skin"};
    pixels.partNames = {"Head"};
    pixels.ramps = {{1, 1, 2, 2, 3}};
    pixels.loops = false;
    auto asset = PixelatorFramesOf(pixels);
    asset.id = "asset:Graphics/Hero";
    CHECK(DecodePixelatorAsset(EncodePixelatorAsset(asset)) == asset);
    auto json = EncodePixelatorAsset(asset);
    json["ramps"][0] = {1, 2, 3};
    CHECK_THROWS(DecodePixelatorAsset(json));
    json = EncodePixelatorAsset(asset);
    json["inkNames"] = 3;
    CHECK_THROWS(DecodePixelatorAsset(json));

    const auto summary = PixelatorSummary(asset, 0);
    CHECK(summary.at("inks")[0].at("name") == "Line");
    CHECK(summary.at("loops") == false);
}
