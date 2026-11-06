#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include "Graphics/Text/FontFace.h"
#include "Tests/PaintHarness.h"

using Catch::Approx;

namespace {
const FontFace& Latin() {
    static const FontFace font = FontFace::FromFile(ENGINE_TEST_FONT);
    return font;
}
FontFace Script(const char* file) { return FontFace::FromFile(std::string(ENGINE_TEST_SCRIPT_FONTS) + "/" + file); }
const FontFace& Arabic() {
    static const FontFace font = Script("NotoSansArabic-Regular.ttf");
    return font;
}
// Latin first, Arabic and Hebrew for what it lacks: a game's menu font with fallbacks.
const FontFace& LatinWithFallbacks() {
    static const FontFace font = [] {
        FontFace made = FontFace::FromFile(ENGINE_TEST_FONT);
        made.AddFallback(Script("NotoSansArabic-Regular.ttf"));
        made.AddFallback(Script("NotoSansHebrew-Regular.ttf"));
        return made;
    }();
    return font;
}
int GlyphOf(const FontFace& font, const char* text) {
    const TextBlock block = font.Layout(text, {});
    REQUIRE(block.Glyphs().size() == 1);
    return block.Glyphs()[0].index;
}
std::vector<int> Indices(const TextBlock& block) {
    std::vector<int> indices;
    for (const TextBlock::Glyph& glyph : block.Glyphs())
        indices.push_back(glyph.index);
    return indices;
}
bool LeftToRight(const TextBlock& block) {
    const auto& glyphs = block.Glyphs();
    return std::is_sorted(glyphs.begin(), glyphs.end(),
                          [](const TextBlock::Glyph& a, const TextBlock::Glyph& b) { return a.x < b.x; });
}

// U+0633 seen, U+0644 lam, U+0627 alef, U+0645 meem: "salam".
const char* const Salam = "\xD8\xB3\xD9\x84\xD8\xA7\xD9\x85";
const char* const Seen = "\xD8\xB3";
const char* const Lam = "\xD9\x84";
const char* const Meem = "\xD9\x85";
} // namespace

TEST_CASE("Engine.Text.ArabicLettersJoinAndReadRightToLeft", "[engine][paint][text]") {
    const TextBlock word = Arabic().Layout(Salam, {.size = 30.0f});
    REQUIRE(word.Lines().size() == 1);
    REQUIRE(word.Glyphs().size() == 3); // Lam and alef become one ligature
    CHECK(LeftToRight(word));
    // Read right to left: seen is drawn last, joined to lam (its initial form). Alef never joins the letter after it,
    // so meem, drawn first, keeps its own form.
    CHECK(word.Glyphs().back().index != GlyphOf(Arabic(), Seen));
    CHECK(word.Glyphs().front().index == GlyphOf(Arabic(), Meem));
    CHECK(word.Glyphs()[1].index != GlyphOf(Arabic(), Lam));
    CHECK(word.Lines()[0].width == Approx(Arabic().Measure(Salam, 30.0f).x));
}

TEST_CASE("Engine.Text.MixedDirectionsKeepEachRunInReadingOrder", "[engine][paint][text]") {
    const FontFace& font = LatinWithFallbacks();
    // A left-to-right line: the Latin word first, then the Arabic word reversed into place on its right.
    const std::string latinFirst = std::string("abc ") + Salam;
    const TextBlock mixed = font.Layout(latinFirst, {.size = 20.0f});
    REQUIRE(mixed.Glyphs().size() == 6);
    CHECK(LeftToRight(mixed));
    CHECK(Indices(mixed)[0] == GlyphOf(Latin(), "a"));
    CHECK(Indices(mixed)[2] == GlyphOf(Latin(), "c"));
    CHECK(mixed.Glyphs()[0].font == 0);
    CHECK(mixed.Glyphs()[3].font == 1); // The Arabic fallback

    // A right-to-left line (it starts with Arabic): the number keeps its own order, on the left of the word.
    const std::string arabicFirst = std::string(Salam) + " 12";
    const TextBlock number = font.Layout(arabicFirst, {.size = 20.0f});
    REQUIRE(number.Glyphs().size() == 5);
    CHECK(LeftToRight(number));
    CHECK(Indices(number)[0] == GlyphOf(Latin(), "1"));
    CHECK(Indices(number)[1] == GlyphOf(Latin(), "2"));
    CHECK(number.Glyphs()[2].font == 1);

    // Hebrew does not join, so the order alone shows: alef, bet, gimel are drawn gimel, bet, alef.
    const TextBlock hebrew = font.Layout("\xD7\x90\xD7\x91\xD7\x92", {.size = 20.0f});
    REQUIRE(hebrew.Glyphs().size() == 3);
    CHECK(hebrew.Glyphs()[0].font == 2);
    CHECK(Indices(hebrew)[0] == GlyphOf(font, "\xD7\x92"));
    CHECK(Indices(hebrew)[2] == GlyphOf(font, "\xD7\x90"));
}

TEST_CASE("Engine.Text.MarksSitOnTheirLetters", "[engine][paint][text]") {
    // Beh with kasra (a vowel mark below): the font moves the mark down onto its letter.
    const TextBlock marked = Arabic().Layout("\xD8\xA8\xD9\x90", {.size = 40.0f});
    REQUIRE(marked.Glyphs().size() == 2);
    CHECK(std::any_of(marked.Glyphs().begin(), marked.Glyphs().end(),
                      [](const TextBlock::Glyph& glyph) { return glyph.y > 0.0f; }));
    CHECK(marked.Lines()[0].width == Approx(Arabic().Measure("\xD8\xA8", 40.0f).x)); // The mark takes no room
    // Latin text has no offsets.
    for (const TextBlock::Glyph& glyph : Latin().Layout("Sample text", {}).Glyphs())
        CHECK(glyph.y == 0.0f);
    // A combining accent becomes the precomposed letter the font has.
    CHECK(Indices(Latin().Layout("e\xCC\x81", {})) == Indices(Latin().Layout("\xC3\xA9", {})));
}

TEST_CASE("Engine.Text.LigaturesJoinUnlessLettersAreSpaced", "[engine][paint][text]") {
    CHECK(Latin().Layout("office", {}).Glyphs().size() == 4); // o, ffi, c, e
    CHECK(Latin().Layout("office", {.letterSpacing = 2.0f}).Glyphs().size() == 6);
    CHECK(Latin().Measure("office", 20.0f, 2.0f).x == Approx(Latin().Measure("office", 20.0f).x + 10.0f).margin(0.5f));
}

TEST_CASE("Engine.Text.LinesBreakWhereUnicodeAllows", "[engine][paint][text]") {
    const FontFace& font = Latin();
    // After a hyphen, not inside the word that follows.
    const TextStyle narrow{.size = 20.0f, .maxWidth = font.Measure("well-kno", 20.0f).x, .wrap = TextWrap::Word};
    const TextBlock hyphen = font.Layout("well-known", narrow);
    REQUIRE(hyphen.Lines().size() == 2);
    CHECK(hyphen.Lines()[0].glyphCount == 5);
    CHECK(hyphen.Lines()[0].width == Approx(font.Measure("well-", 20.0f).x));

    // Chinese has no spaces; a line may break between any two characters, so the widest piece is one character and
    // a shrink to fit keeps the size (the font lacks them: they draw as '?', which does not change where lines break).
    const std::string chinese = "\xE4\xB8\xAD\xE6\x96\x87\xE5\xAD\x97\xE4\xB8\xAD\xE6\x96\x87\xE5\xAD\x97";
    const float character = font.Measure("?", 20.0f).x;
    const TextBlock wrapped = font.Layout(
        chinese, {.size = 20.0f, .maxWidth = character * 2.5f, .wrap = TextWrap::Word, .fit = TextFit::Shrink});
    CHECK(wrapped.Size() == 20.0f);
    REQUIRE(wrapped.Lines().size() == 3);
    for (const TextBlock::Line& line : wrapped.Lines())
        CHECK(line.glyphCount == 2);

    // Thai has no spaces either and no dictionary here: it wraps between characters, keeping marks on their letters.
    const FontFace thai = Script("NotoSansThai-Regular.ttf");
    const char* const hello = "\xE0\xB8\xAA\xE0\xB8\xA7\xE0\xB8\xB1\xE0\xB8\xAA\xE0\xB8\x94\xE0\xB8\xB5"
                              "\xE0\xB8\x84\xE0\xB8\xA3\xE0\xB8\xB1\xE0\xB8\x9A";
    const TextBlock line = thai.Layout(hello, {.size = 20.0f});
    const TextStyle thin{.size = 20.0f, .maxWidth = line.Extent().x * 0.4f, .wrap = TextWrap::Word};
    const TextBlock lines = thai.Layout(hello, thin);
    CHECK(lines.Lines().size() >= 3);
    CHECK(lines.Glyphs().size() == line.Glyphs().size());
    for (const TextBlock::Line& each : lines.Lines())
        CHECK(each.width <= thin.maxWidth + 1e-3f);
}

TEST_CASE("Engine.Text.FallbackFontsDrawWhatTheFontLacks", "[engine][paint][text]") {
    CHECK_FALSE(Latin().HasCharacter(0x0633));
    CHECK(LatinWithFallbacks().HasCharacter(0x0633));
    CHECK(LatinWithFallbacks().HasCharacter(U'a'));
    CHECK_FALSE(LatinWithFallbacks().HasCharacter(0xE000));
    // Lines keep the first font's height; a character no font has is the first font's '?'.
    CHECK(LatinWithFallbacks().LineHeight(20.0f) == Approx(Latin().LineHeight(20.0f)));
    const TextBlock missing = LatinWithFallbacks().Layout("\xEE\x80\x80", {});
    REQUIRE(missing.Glyphs().size() == 1);
    CHECK(missing.Glyphs()[0].font == 0);
    CHECK(missing.Glyphs()[0].index == GlyphOf(Latin(), "?"));

    PaintHarness h;
    h.Begin();
    h.paint.Text(LatinWithFallbacks(), std::string("a") + Seen, {10.0f, 10.0f}, {.size = 24.0f});
    h.End();
    CHECK(h.Drawn().size() == 12); // Two glyph quads, one from each font
}
