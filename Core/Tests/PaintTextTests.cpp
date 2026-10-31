#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include "Graphics/Text/FontFace.h"
#include "Graphics/Text/Internals/AtlasPacker.h"
#include "Graphics/Text/Internals/FontData.h"
#include "Utilities/Utf8.h"
#include "Tests/PaintHarness.h"

using Catch::Approx;

namespace {
const FontFace& TestFont() {
    static const FontFace font = FontFace::FromFile(ENGINE_TEST_FONT);
    return font;
}

std::vector<PaintKind> Kinds(const std::vector<PaintVertex>& vertices) {
    std::vector<PaintKind> kinds;
    for (std::size_t i = 0; i < vertices.size(); i += 6)
        if (kinds.empty() || kinds.back() != vertices[i].kind) kinds.push_back(vertices[i].kind);
    return kinds;
}
} // namespace

TEST_CASE("Engine.Text.Utf8DecodesAndReplacesMalformedBytes", "[engine][paint][text]") {
    const std::string text = "a\xC4\x9F\xE2\x82\xAC\xF0\x9F\x98\x80\xFF\xC4";
    std::vector<char32_t> decoded;
    for (std::size_t at = 0; at < text.size();)
        decoded.push_back(NextCodepoint(text, at));
    CHECK(decoded == std::vector<char32_t>{U'a', 0x011F, 0x20AC, 0x1F600, 0xFFFD, 0xFFFD});
}

TEST_CASE("Engine.Text.MeasureScalesWithSizeAndCountsLines", "[engine][paint][text]") {
    const FontFace& font = TestFont();
    const Float2 small = font.Measure("Snake", 20.0f);
    const Float2 large = font.Measure("Snake", 40.0f);
    CHECK(small.x > 0.0f);
    CHECK(large.x == Approx(small.x * 2.0f).epsilon(1e-4));
    CHECK(small.y == Approx(20.0f));
    CHECK(font.Measure("A\nB", 20.0f).y == Approx(font.LineHeight(20.0f) + 20.0f));
    CHECK(font.LineHeight(20.0f) >= 20.0f);
    CHECK(font.Measure("ab", 20.0f, 3.0f).x == Approx(font.Measure("ab", 20.0f).x + 3.0f));
}

TEST_CASE("Engine.Text.WordWrapKeepsLinesWithinTheWidth", "[engine][paint][text]") {
    const FontFace& font = TestFont();
    const float word = font.Measure("snake", 20.0f).x;
    TextStyle style{.size = 20.0f, .maxWidth = word * 2.5f, .wrap = TextWrap::Word};
    const TextBlock block = font.Layout("snake snake snake snake", style);
    REQUIRE(block.Lines().size() == 2);
    for (const TextBlock::Line& line : block.Lines()) {
        CHECK(line.width <= style.maxWidth);
        CHECK(line.glyphCount == 10);
        CHECK(block.Glyphs()[line.firstGlyph].x == Approx(0.0f));
    }
    CHECK(block.Lines()[1].top == Approx(font.LineHeight(20.0f)));
    style.lineHeight = 31.0f; // Absolute pitch replaces the font's line height
    CHECK(font.Layout("snake snake snake snake", style).Lines()[1].top == Approx(31.0f));
    style.lineHeight = 0.0f;
    style.maxWidth = word * 0.5f; // A word longer than the line breaks between characters
    const TextBlock broken = font.Layout("snake", style);
    CHECK(broken.Lines().size() >= 2);
    for (const TextBlock::Line& line : broken.Lines())
        CHECK(line.width <= style.maxWidth + 1e-3f);
}

TEST_CASE("Engine.Text.ShrinkFitsTheWidestLine", "[engine][paint][text]") {
    const FontFace& font = TestFont();
    const float width = font.Measure("A long title", 40.0f).x;
    const TextBlock fitted =
        font.Layout("A long title", {.size = 40.0f, .maxWidth = width * 0.5f, .fit = TextFit::Shrink});
    CHECK(fitted.Size() == Approx(20.0f).epsilon(1e-3));
    CHECK(fitted.Extent().x <= width * 0.5f + 1e-3f);
    const TextBlock roomy =
        font.Layout("A long title", {.size = 40.0f, .maxWidth = width * 2.0f, .fit = TextFit::Shrink});
    CHECK(roomy.Size() == 40.0f); // Never grows
    CHECK(font.FittedSize("A long title", 40.0f, width * 0.5f) == Approx(fitted.Size()));
    CHECK(font.FittedSize("A long title", 40.0f, width * 2.0f) == 40.0f);
    CHECK(font.FittedSize("A long title", 40.0f, 0.0f) == 40.0f); // No limit
}

TEST_CASE("Engine.Text.ShrunkTextKeepsItsVerticalCentre", "[engine][paint][text]") {
    const FontFace& font = TestFont();
    const float width = font.Measure("Title", 40.0f).x;
    const auto middle = [&](float maxWidth) {
        PaintHarness h;
        h.Begin();
        h.paint.Text(font, "Title", {10.0f, 20.0f}, {.size = 40.0f, .maxWidth = maxWidth, .fit = TextFit::Shrink});
        h.End();
        float top = 1e9f;
        float bottom = -1e9f;
        for (const PaintVertex& vertex : h.Drawn()) {
            top = std::min(top, vertex.y);
            bottom = std::max(bottom, vertex.y);
        }
        return std::pair{(top + bottom) * 0.5f, bottom - top};
    };
    const auto [fullMiddle, fullHeight] = middle(0.0f);
    const auto [shrunkMiddle, shrunkHeight] = middle(width * 0.5f);
    CHECK(shrunkHeight < fullHeight * 0.75f);
    CHECK(shrunkMiddle == Approx(fullMiddle).margin(fullHeight * 0.05f));
}

TEST_CASE("Engine.Text.MissingCharactersFallBackToQuestionMark", "[engine][paint][text]") {
    const FontFace& font = TestFont();
    CHECK_FALSE(font.HasCharacter(0xE000));
    const TextBlock missing = font.Layout("\xEE\x80\x80", {});
    const TextBlock question = font.Layout("?", {});
    REQUIRE(missing.Glyphs().size() == 1);
    CHECK(missing.Glyphs()[0].index == question.Glyphs()[0].index);
}

TEST_CASE("Engine.Text.DrawsInOneBatchAndCachesGlyphs", "[engine][paint][text]") {
    PaintHarness h;
    h.Begin();
    h.paint.Text(TestFont(), "Score 120", {10.0f, 10.0f}, {.size = 16.0f});
    h.paint.Circle({50.0f, 50.0f}, 5.0f, Fill({255, 0, 0, 255}));
    h.paint.Text(TestFont(), "Score 120", {10.0f, 40.0f}, {.size = 16.0f});
    CHECK(h.glyphs.RasterizedThisFrame() > 0);
    CHECK(h.layouts.HitsThisFrame() == 1);
    CHECK(h.layouts.MissesThisFrame() == 1);
    h.End();
    REQUIRE(h.Draws().size() == 1);
    CHECK(h.layouts.Size() == 1);
    CHECK(h.layouts.HitsThisFrame() == 0);
    CHECK(h.layouts.MissesThisFrame() == 0);

    h.Begin();
    h.paint.Text(TestFont(), "Score 120", {10.0f, 10.0f}, {.size = 16.0f});
    CHECK(h.glyphs.RasterizedThisFrame() == 0); // Same size on screen: everything cached
    h.paint.Text(TestFont(), "Score 120", {10.0f, 10.0f}, {.size = 24.0f});
    CHECK(h.glyphs.RasterizedThisFrame() > 0);
    CHECK(h.layouts.HitsThisFrame() == 1);
    CHECK(h.layouts.MissesThisFrame() == 1);
    h.End();
}

TEST_CASE("Engine.Text.TextWithoutAFontUsesTheGamesFont", "[engine][paint][text]") {
    PaintHarness h;
    h.Begin();
    CHECK_FALSE(h.paint.HasFont());
    CHECK_THROWS_AS(h.paint.Text("Score", {10.0f, 10.0f}, {.size = 16.0f}), std::logic_error);
    h.context.font = &TestFont();
    REQUIRE(h.paint.HasFont());
    CHECK(h.paint.Font().Id() == TestFont().Id());
    CHECK(h.paint.Text("Score", {10.0f, 10.0f}, {.size = 16.0f}).x == Approx(TestFont().Measure("Score", 16.0f).x));
    const Float2 box =
        h.paint.TextBox("Score Score", {10.0f, 40.0f, TestFont().Measure("Score", 16.0f).x + 1.0f, 30.0f},
                        {.size = 16.0f, .wrap = TextWrap::Word});
    CHECK(box.y == Approx(TestFont().LineHeight(16.0f) + 16.0f)); // Two lines
    h.End();
    CHECK(h.Draws().size() == 1);
}

TEST_CASE("Engine.Text.ExactSizeGlyphsLandOnThePixelGrid", "[engine][paint][text]") {
    PaintHarness h({400.0f, 200.0f}, 2.0f);
    h.Begin();
    h.paint.Text(TestFont(), "Hi", {10.3f, 10.7f}, {.size = 15.0f});
    h.End();
    for (const PaintVertex& vertex : h.Drawn()) {
        CHECK(vertex.x * 2.0f == Approx(std::round(vertex.x * 2.0f)).margin(1e-3));
        CHECK(vertex.y * 2.0f == Approx(std::round(vertex.y * 2.0f)).margin(1e-3));
    }
}

TEST_CASE("Engine.Text.EffectsDrawUnderTheGlyphsInOneBatch", "[engine][paint][text]") {
    PaintHarness h;
    h.Begin();
    h.paint.Text(TestFont(), "Neon", {20.0f, 20.0f},
                 {.size = 32.0f,
                  .glow = {8.0f, {0, 255, 255, 255}},
                  .outline = {2.0f, {0, 0, 0, 255}},
                  .shadow = {{2.0f, 2.0f}, {0, 0, 0, 160}}});
    h.End();
    REQUIRE(h.Draws().size() == 1);
    const auto vertices = h.Drawn();
    // Shadow, halo, outline (both distance glyphs), core; four glyphs each.
    CHECK(Kinds(vertices) == std::vector<PaintKind>{PaintKind::Glyph, PaintKind::GlyphHalo, PaintKind::Glyph});
    CHECK(vertices[24].flags == static_cast<std::uint8_t>(PaintMode::Halo));
    CHECK(vertices[24].color.a == 0); // The halo adds light
    CHECK(vertices[48].flags == static_cast<std::uint8_t>(PaintMode::Stroke));
}

TEST_CASE("Engine.Text.AlignmentPlacesLinesAroundThePosition", "[engine][paint][text]") {
    PaintHarness h;
    const float width = TestFont().Measure("Wide", 20.0f).x;
    for (const TextAlign align : {TextAlign::Left, TextAlign::Center, TextAlign::Right}) {
        h.Begin();
        h.paint.Text(TestFont(), "Wide", {200.0f, 50.0f}, {.size = 20.0f, .align = align});
        h.End();
        float left = 1e9f;
        float right = -1e9f;
        for (const PaintVertex& vertex : h.Drawn()) {
            left = std::min(left, vertex.x);
            right = std::max(right, vertex.x);
        }
        const float middle = (left + right) * 0.5f;
        const float expected = align == TextAlign::Left    ? 200.0f + width * 0.5f
                               : align == TextAlign::Right ? 200.0f - width * 0.5f
                                                           : 200.0f;
        CHECK(middle == Approx(expected).margin(2.0f));
    }
}

TEST_CASE("Engine.Text.AtlasPackerKeepsRectanglesApartAndInside", "[engine][paint][text]") {
    AtlasPacker packer(64, 64, 1);
    std::vector<PixelRect> placed;
    while (const auto rect = packer.Insert(7, 5))
        placed.push_back(*rect);
    CHECK(placed.size() >= 40);
    for (std::size_t i = 0; i < placed.size(); ++i) {
        const PixelRect& a = placed[i];
        CHECK(a.x >= 1);
        CHECK(a.y >= 1);
        CHECK(a.x + a.width <= 64);
        CHECK(a.y + a.height <= 64);
        for (std::size_t j = i + 1; j < placed.size(); ++j) {
            const PixelRect& b = placed[j];
            const bool apart = a.x + a.width + 1 <= b.x || b.x + b.width + 1 <= a.x || a.y + a.height + 1 <= b.y ||
                               b.y + b.height + 1 <= a.y;
            CHECK(apart);
        }
    }
    packer.Clear();
    CHECK(packer.Insert(62, 62).has_value());
    CHECK_FALSE(packer.Insert(1, 1).has_value());
}

TEST_CASE("Engine.Text.FullCacheRecyclesPagesUnusedThisFrame", "[engine][paint][text]") {
    NullDevice device;
    GlyphCache cache(device, 64, 1);
    const FontData& data = FontAccess::Data(TestFont());
    cache.BeginFrame();
    for (char32_t c = U'A'; c <= U'Z'; ++c)
        (void)cache.Coverage(data, data.GlyphIndex(c), 20);
    const std::size_t sameFrame = cache.Pages();
    CHECK(sameFrame > 1); // Pages in use this frame are never overwritten: the budget is exceeded instead
    cache.BeginFrame();
    (void)cache.Coverage(data, data.GlyphIndex(U'a'), 30);
    (void)cache.Coverage(data, data.GlyphIndex(U'b'), 30);
    CHECK(cache.Pages() == sameFrame); // Old pages are recycled first
    CHECK(device.LiveTextures() == sameFrame);
}
