#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>
#include "Graphics/Pixel/PixelCanvas.h"
#include "Graphics/Pixel/PixelLegend.h"
#include "Graphics/Pixel/PixelMask.h"
#include "Graphics/Pixel/PixelPalette.h"
#include "Graphics/Pixel/PixelSprite.h"
#include "Graphics/Pixel/PixelSpriteSet.h"
#include "Tests/PaintHarness.h"

namespace {
constexpr InkRamp Ramp{1, 2, 3, 4, 5};

// One string per row; '.' is the clear ink, digits and letters as the default legend.
std::vector<std::string> Rows(const PixelCanvas& canvas) {
    std::vector<std::string> rows;
    for (int y = 0; y < canvas.Height(); ++y) {
        std::string row;
        for (int x = 0; x < canvas.Width(); ++x) {
            const Ink ink = canvas.Get(x, y);
            row += ink == ClearInk ? '.' : ink < 10 ? static_cast<char>('0' + ink) : static_cast<char>('a' + ink - 10);
        }
        rows.push_back(row);
    }
    return rows;
}

std::vector<std::string> Rows(const PixelMask& mask) {
    std::vector<std::string> rows;
    for (int y = 0; y < mask.Height(); ++y) {
        std::string row;
        for (int x = 0; x < mask.Width(); ++x)
            row += mask.Has(x, y) ? '#' : '.';
        rows.push_back(row);
    }
    return rows;
}

int SpriteMade = 0;
PixelSprite MakeNumbered(std::size_t index) {
    ++SpriteMade;
    PixelCanvas canvas(2, 1);
    canvas.Set(0, 0, static_cast<Ink>(index + 1));
    return PixelSprite(canvas);
}
} // namespace

TEST_CASE("Engine.Pixel.PaletteNumbersInksFromOne", "[engine][pixel]") {
    const PixelPalette palette{Rgb(0x112233), Rgb(0x445566)};
    CHECK(palette.Count() == 3);
    CHECK(palette[ClearInk].a == 0);
    CHECK(palette[1] == Rgb(0x112233));
    CHECK(palette[2] == Rgb(0x445566));
    CHECK(palette[200].a == 0);

    PixelPalette grown = palette;
    CHECK(grown.Revision() == palette.Revision());
    CHECK(grown.Add(Rgb(0x778899)) == 3);
    CHECK(grown.Revision() != palette.Revision());
    CHECK_THROWS_AS(grown.Set(ClearInk, Rgb(0)), std::invalid_argument);
    CHECK_THROWS_AS(grown.Set(4, Rgb(0)), std::invalid_argument);

    const std::vector<Color4> full(255, Rgb(0x010203));
    PixelPalette most(full);
    CHECK(most.Count() == MaxInks);
    CHECK_THROWS_AS(most.Add(Rgb(0)), std::invalid_argument);
    const std::vector<Color4> tooMany(256, Rgb(0));
    CHECK_THROWS_AS(PixelPalette(tooMany), std::invalid_argument);
}

TEST_CASE("Engine.Pixel.MixedPaletteKeepsAlpha", "[engine][pixel]") {
    const PixelPalette palette{Color4{0, 0, 0, 255}, Color4{0, 0, 0, 128}};
    const PixelPalette flashed = palette.Mixed({255, 255, 255, 255}, 1.0f);
    CHECK(flashed[1] == Color4{255, 255, 255, 255});
    CHECK(flashed[2] == Color4{255, 255, 255, 128});
    CHECK(flashed[ClearInk].a == 0);
    CHECK(flashed.Revision() != palette.Revision());
    CHECK(palette.Mixed({255, 255, 255, 255}, 0.0f).Revision() == palette.Revision());
}

TEST_CASE("Engine.Pixel.LegendMapsDigitsAndLetters", "[engine][pixel]") {
    const PixelLegend legend;
    CHECK_FALSE(legend.Find('.').has_value());
    CHECK_FALSE(legend.Find(' ').has_value());
    CHECK(legend.Find('0') == ClearInk);
    CHECK(legend.Find('9') == 9);
    CHECK(legend.Find('a') == 10);
    CHECK(legend.Find('z') == 35);
    CHECK(legend.Find('A') == 36);
    CHECK_THROWS_AS(legend.Find('#'), std::invalid_argument);

    const PixelLegend own{{'x', 7}};
    CHECK(own.Find('x') == 7);
    CHECK_THROWS_AS(own.Find('1'), std::invalid_argument);
    PixelLegend changed;
    CHECK_THROWS_AS(changed.Set('.', 1), std::invalid_argument);
}

TEST_CASE("Engine.Pixel.MaskCoversPixelCentres", "[engine][pixel]") {
    PixelMask square(5, 4);
    square.Poly({{1, 1}, {4, 1}, {4, 3}, {1, 3}});
    CHECK(Rows(square) == std::vector<std::string>{".....", ".###.", ".###.", "....."});
    CHECK(square.Count() == 6);

    PixelMask round(5, 5);
    round.Ellipse(2.5f, 2.5f, 2.0f, 2.0f);
    CHECK(Rows(round) == std::vector<std::string>{"..#..", ".###.", "#####", ".###.", "..#.."});

    PixelMask line(4, 4);
    line.Line(0, 0, 3, 3);
    CHECK(Rows(line) == std::vector<std::string>{"#...", ".#..", "..#.", "...#"});

    PixelMask diagonal(5, 4);
    diagonal.Line(0, 0, 3, 3);
    PixelMask cut = square;
    cut.Remove(diagonal);
    CHECK(Rows(cut)[1] == "..##.");
    CHECK_THROWS_AS(cut.Add(line), std::invalid_argument);
    CHECK_THROWS_AS(PixelMask(-1, 2), std::invalid_argument);
}

TEST_CASE("Engine.Pixel.ShadeLightsTheSideFacingTheLight", "[engine][pixel]") {
    PixelCanvas canvas(6, 6); // Light from the top left
    PixelMask mask(6, 6);
    mask.Rect(1, 1, 4, 4);
    canvas.Shade(mask, Ramp, 1);
    CHECK(Rows(canvas) == std::vector<std::string>{"......", ".5555.", ".5331.", ".5321.", ".5111.", "......"});

    PixelCanvas fromRight(6, 6, {.light = {1, 0}});
    fromRight.Shade(mask, Ramp, 1);
    CHECK(Rows(fromRight)[2] == ".1235."); // The dither marks every other pixel two steps from the dark side
}

TEST_CASE("Engine.Pixel.EdgedVolumeSeparatesFromWhatIsBehind", "[engine][pixel]") {
    PixelCanvas canvas(5, 3);
    canvas.FlatRect(0, 0, 5, 3, 9);
    PixelMask mask(5, 3);
    mask.Rect(2, 1, 1, 1);
    canvas.Shade(mask, Ramp, 1);
    CHECK(Rows(canvas) == std::vector<std::string>{"99199", "91519", "99199"});

    PixelCanvas styled(5, 3, {.edge = 7});
    styled.FlatRect(0, 0, 5, 3, 9);
    styled.Shade(mask, Ramp, 1);
    CHECK(Rows(styled)[1] == "97579");

    PixelCanvas loose(5, 3);
    loose.FlatRect(0, 0, 5, 3, 9);
    loose.Shade(mask, Ramp, 1, false);
    CHECK(Rows(loose)[0] == "99999");
}

TEST_CASE("Engine.Pixel.PartScopesNameWhatTheyDraw", "[engine][pixel]") {
    PixelCanvas canvas(4, 1);
    canvas.Set(0, 0, 1);
    {
        auto helmet = canvas.Part(3);
        canvas.Set(1, 0, 1);
        {
            auto plume = canvas.Part(4);
            canvas.Set(2, 0, 1);
        }
        CHECK(canvas.CurrentPart() == 3);
        canvas.Set(3, 0, 1);
    }
    CHECK(canvas.CurrentPart() == NoPart);
    CHECK(canvas.PartAt(0, 0) == NoPart);
    CHECK(canvas.PartAt(1, 0) == 3);
    CHECK(canvas.PartAt(2, 0) == 4);
    CHECK(canvas.PartAt(3, 0) == 3);
    auto again = canvas.Part(5);
    canvas.Set(1, 0, ClearInk);
    CHECK(canvas.PartAt(1, 0) == NoPart);
}

TEST_CASE("Engine.Pixel.StampSpeckleOutlinePasteAndFlip", "[engine][pixel]") {
    PixelCanvas canvas(5, 3);
    canvas.Stamp(1, 0, {"12", ".a", "0"});
    CHECK(Rows(canvas) == std::vector<std::string>{".12..", "..a..", "....."});
    CHECK_THROWS_AS(canvas.Stamp(0, 0, {"#"}), std::invalid_argument);

    canvas.Outline(9);
    CHECK(Rows(canvas) == std::vector<std::string>{"9129.", ".9a9.", "..9.."});

    PixelCanvas flipped = canvas;
    flipped.FlipX();
    CHECK(Rows(flipped)[0] == ".9219");

    PixelCanvas target(4, 2);
    auto part = target.Part(2);
    target.Paste(canvas, -1, 0);
    CHECK(Rows(target) == std::vector<std::string>{"129.", "9a9."});
    CHECK(target.PartAt(0, 0) == 2);

    PixelCanvas noise(16, 16);
    noise.FlatRect(0, 0, 16, 16, 1);
    PixelCanvas same = noise;
    noise.Speckle(1, 2, 30, 7);
    same.Speckle(1, 2, 30, 7);
    CHECK(Rows(noise) == Rows(same));
    int changed = 0;
    for (const Ink ink : noise.Inks())
        changed += ink == 2;
    CHECK(changed > 40);
    CHECK(changed < 120);
}

TEST_CASE("Engine.Pixel.SpritesShareTheirPixels", "[engine][pixel]") {
    PixelCanvas canvas(3, 2);
    {
        auto part = canvas.Part(6);
        canvas.Set(2, 1, 4);
    }
    const PixelSprite sprite(canvas, {1, 2});
    const PixelSprite copy = sprite; // NOLINT(performance-unnecessary-copy-initialization)
    CHECK(sprite.Width() == 3);
    CHECK(sprite.Height() == 2);
    CHECK(sprite.Pivot() == Vec2{1, 2});
    CHECK(sprite.At(2, 1) == 4);
    CHECK(sprite.PartAt(2, 1) == 6);
    CHECK(sprite.At(5, 5) == ClearInk);
    CHECK(copy.Id() == sprite.Id());
    CHECK(PixelSprite(canvas).Id() != sprite.Id());
    CHECK(PixelSprite().Empty());
    CHECK(PixelSprite().Id() == 0);
}

TEST_CASE("Engine.Pixel.SpriteSetMakesEachSpriteOnce", "[engine][pixel]") {
    SpriteMade = 0;
    const PixelSpriteSet set(3, &MakeNumbered);
    CHECK(SpriteMade == 0);
    CHECK(set[2].At(0, 0) == 3);
    const std::uint64_t id = set[2].Id();
    CHECK(set[2].Id() == id);
    CHECK(SpriteMade == 1);
    CHECK_THROWS_AS(set[3], std::out_of_range);
    CHECK_THROWS_AS(PixelSpriteSet(1, nullptr), std::invalid_argument);
}

TEST_CASE("Engine.Pixel.SpriteDrawsInksThroughItsPalette", "[engine][pixel]") {
    PaintHarness h({100.0f, 100.0f}, 2.0f);
    PixelCanvas canvas(3, 2);
    canvas.Set(0, 0, 1);
    canvas.Set(2, 1, 2);
    const PixelSprite sprite(canvas, {1, 2});
    const PixelPalette palette{Color4{255, 0, 0, 255}, Color4{0, 255, 0, 128}};

    h.Begin();
    h.paint.Sprite(sprite, palette, {10.25f, 20.0f}, {.scale = 3.0f, .flash = {0, 0, 255, 255}, .flashAmount = 0.5f});
    h.End();
    REQUIRE(h.Draws().size() == 1);
    const auto& bound = h.device.CommandsOf(NullCommandType::BindTextures);
    REQUIRE_FALSE(bound.empty());
    const std::vector<GpuTexture>& textures = bound.back()->textures;
    REQUIRE(textures.size() == 2);
    const GpuTextureDesc& inks = h.device.Describe(textures[0]);
    const GpuTextureDesc& colors = h.device.Describe(textures[1]);
    CHECK(inks.format == GpuFormat::R8);
    CHECK(inks.filter == GpuFilter::Nearest);
    CHECK(colors.format == GpuFormat::Rgba8);
    CHECK(colors.filter == GpuFilter::Nearest);
    // Premultiplied palette row: ink 2 is half-transparent green.
    const auto& row = h.device.Pixels(textures[1]);
    CHECK(static_cast<int>(row[4]) == 255);
    CHECK(static_cast<int>(row[9]) == 128);
    CHECK(static_cast<int>(row[11]) == 128);

    const std::vector<PaintVertex> quad = h.Drawn();
    REQUIRE(quad.size() == 6);
    for (const PaintVertex& vertex : quad) {
        CHECK(vertex.kind == PaintKind::Indexed);
        CHECK(vertex.style0 == 0.5f);
        CHECK(vertex.color2.b == 255);
    }
    // Pivot (1, 2) at (10.25, 20): left 7.25 snaps to 7.0 (two device pixels a unit), right 16.25 to 16.5? No: 7.25
    // and 16.25 are 14.5 and 32.5 device pixels, which round to 15 and 33, so 7.5 and 16.5.
    float left = 1e9f;
    float right = -1e9f;
    float top = 1e9f;
    float bottom = -1e9f;
    for (const PaintVertex& vertex : quad) {
        left = std::min(left, vertex.x);
        right = std::max(right, vertex.x);
        top = std::min(top, vertex.y);
        bottom = std::max(bottom, vertex.y);
    }
    CHECK(left == 7.5f);
    CHECK(right == 16.5f);
    CHECK(top == 14.0f);
    CHECK(bottom == 20.0f);
}

TEST_CASE("Engine.Pixel.FlippedSpriteMirrorsAroundItsPivot", "[engine][pixel]") {
    PaintHarness h({100.0f, 100.0f}, 1.0f);
    PixelCanvas canvas(4, 1);
    canvas.Set(0, 0, 1);
    const PixelSprite sprite(canvas, {1, 1});
    const PixelPalette palette{Color4{255, 255, 255, 255}};
    h.Begin();
    h.paint.Sprite(sprite, palette, {50.0f, 50.0f}, {.flipX = true});
    h.End();
    const std::vector<PaintVertex> quad = h.Drawn();
    float left = 1e9f;
    float right = -1e9f;
    for (const PaintVertex& vertex : quad) {
        left = std::min(left, vertex.x);
        right = std::max(right, vertex.x);
        if (vertex.x == left) CHECK(vertex.u > 0.0f); // The left edge samples the sprite's right side
    }
    CHECK(left == 47.0f);
    CHECK(right == 51.0f);
}

TEST_CASE("Engine.Pixel.AtlasUploadsEachSpriteAndPaletteOnce", "[engine][pixel]") {
    PaintHarness h;
    PixelCanvas canvas(8, 8);
    canvas.FlatRect(0, 0, 8, 8, 1);
    const PixelSprite sprite(canvas);
    PixelPalette palette{Color4{255, 255, 255, 255}};
    h.Begin();
    h.paint.Sprite(sprite, palette, {0.0f, 0.0f});
    h.paint.Sprite(sprite, palette, {20.0f, 0.0f});
    CHECK(h.pixels.UploadedThisFrame() == 2);
    h.End();
    h.Begin();
    h.paint.Sprite(sprite, palette, {0.0f, 0.0f});
    CHECK(h.pixels.UploadedThisFrame() == 0);
    palette.Set(1, Color4{0, 0, 0, 255});
    h.paint.Sprite(sprite, palette, {0.0f, 0.0f});
    CHECK(h.pixels.UploadedThisFrame() == 1);
    h.End();
    const auto& inks = h.device.Pixels(h.pixels.Sprite(sprite).page);
    CHECK(inks.size() == static_cast<std::size_t>(h.pixels.PageSize() * h.pixels.PageSize()));
}

TEST_CASE("Engine.Pixel.SpriteWithoutAtlasOrWithBadScaleIsRejected", "[engine][pixel]") {
    PaintHarness h;
    PixelCanvas canvas(2, 2);
    canvas.Set(0, 0, 1);
    const PixelSprite sprite(canvas);
    const PixelPalette palette{Color4{255, 255, 255, 255}};
    h.list.SetStrict(false);
    h.Begin();
    h.paint.Sprite(sprite, palette, {0.0f, 0.0f}, {.scale = -1.0f});
    h.paint.Sprite(PixelSprite(), palette, {0.0f, 0.0f});
    h.context.pixels = nullptr;
    h.paint.Sprite(sprite, palette, {0.0f, 0.0f});
    h.End();
    CHECK(h.list.Rejected() == 2);
    CHECK(h.Draws().empty());
}
