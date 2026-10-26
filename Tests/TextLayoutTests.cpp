#include <stdexcept>
#include <string_view>
#include "Tests/Support/Diagnostics.h"
#include "Graphics/Draw/TextLayout.h"

namespace {
int MeasureGlyphs(const char* text, int fontSize) {
    int glyphs = 0;
    for (unsigned char byte : std::string_view(text))
        if ((byte & 0xc0) != 0x80) ++glyphs;
    return glyphs * fontSize;
}
} // namespace

TEST_CASE("Engine.TextLayout.WrapsWordsAndCollapsesSpaces", "[engine][graphics]") {
    const auto layout = LayoutText("  one  two three four  ", {.fontSize = 10, .maxWidth = 70}, MeasureGlyphs);
    CHECK(layout.nominalFontSize == 10);
    REQUIRE(layout.lines.size() == 3);
    CHECK(layout.lines[0].text == "one two");
    CHECK(layout.lines[0].width == 70);
    CHECK(layout.lines[1].text == "three");
    CHECK(layout.lines[2].text == "four");
    CHECK(layout.lines[2].fontSize == 10);
    CHECK(LayoutText("", {}, MeasureGlyphs).lines.empty());
    CHECK(LayoutText("   ", {}, MeasureGlyphs).lines.empty());
}

TEST_CASE("Engine.TextLayout.PreservesUtf8WordsAndOwnsItsStrings", "[engine][graphics]") {
    std::string label = "şöğ üşğ a";
    const auto layout = LayoutText(label, {.fontSize = 10, .maxWidth = 50}, MeasureGlyphs);
    label.clear();
    REQUIRE(layout.lines.size() == 2);
    CHECK(layout.lines[0].text == "şöğ");
    CHECK(layout.lines[0].width == 30);
    CHECK(layout.lines[1].text == "üşğ a");
    CHECK(layout.lines[1].width == 50);
    const auto substring = LayoutText(std::string_view("one ignored").substr(0, 3), {}, MeasureGlyphs);
    REQUIRE(substring.lines.size() == 1);
    CHECK(substring.lines[0].text == "one");
}

TEST_CASE("Engine.TextLayout.HonorsHardBreaksAndBlankLines", "[engine][graphics]") {
    const auto layout = LayoutText("one\r\n\r\ntwo\n", {.fontSize = 10, .maxWidth = 120}, MeasureGlyphs);
    REQUIRE(layout.lines.size() == 4);
    CHECK(layout.lines[0].text == "one");
    CHECK(layout.lines[1].text.empty());
    CHECK(layout.lines[2].text == "two");
    CHECK(layout.lines[3].text.empty());
    const auto discard = LayoutText("one\ntwo\nthree", {.maxLines = 2}, MeasureGlyphs);
    REQUIRE(discard.lines.size() == 2);
    CHECK(discard.lines[1].text == "two");
    const auto append = LayoutText("one\ntwo\nthree",
                                   {.maxLines = 2, .lineOverflow = TextLineOverflow::AppendToLastLine}, MeasureGlyphs);
    REQUIRE(append.lines.size() == 2);
    CHECK(append.lines[1].text == "two three");
}

TEST_CASE("Engine.TextLayout.KeepsOversizedWordsIntactWithOptionalFitting", "[engine][graphics]") {
    const auto allow = LayoutText("toolong a bb", {.fontSize = 10, .maxWidth = 20}, MeasureGlyphs);
    REQUIRE(allow.lines.size() == 3);
    CHECK(allow.lines[0].text == "toolong");
    CHECK(allow.lines[0].width == 70);
    CHECK(allow.lines[0].fontSize == 10);
    const auto fit =
        LayoutText("toolong a bb", {.fontSize = 10, .maxWidth = 20, .widthOverflow = TextWidthOverflow::ShrinkToFit},
                   MeasureGlyphs);
    REQUIRE(fit.lines.size() == 3);
    CHECK(fit.lines[0].text == "toolong");
    CHECK(fit.lines[0].fontSize == 2);
    CHECK(fit.lines[0].width == 14);
    CHECK(fit.lines[1].fontSize == 10);
    CHECK(fit.lines[2].fontSize == 10);
}

TEST_CASE("Engine.TextLayout.DiscardAndAppendHaveDistinctFinalLines", "[engine][graphics]") {
    const auto discard =
        LayoutText("one two three four five", {.fontSize = 10, .maxWidth = 80, .maxLines = 2}, MeasureGlyphs);
    REQUIRE(discard.lines.size() == 2);
    CHECK(discard.lines[0].text == "one two");
    CHECK(discard.lines[1].text == "three");
    const auto append = LayoutText("one two three four five",
                                   {.fontSize = 10,
                                    .maxWidth = 80,
                                    .maxLines = 2,
                                    .widthOverflow = TextWidthOverflow::ShrinkToFit,
                                    .lineOverflow = TextLineOverflow::AppendToLastLine},
                                   MeasureGlyphs);
    REQUIRE(append.lines.size() == 2);
    CHECK(append.lines[0].text == "one two");
    CHECK(append.lines[0].fontSize == 10);
    CHECK(append.lines[1].text == "three four five");
    CHECK(append.lines[1].fontSize == 5);
    CHECK(append.lines[1].width == 75);
}

TEST_CASE("Engine.TextLayout.AchievementPatchNotesAndHudPolicies", "[engine][graphics]") {
    const auto tile =
        LayoutText("alpha beta gamma delta epsilon", {.fontSize = 20, .maxWidth = 184, .maxLines = 2}, MeasureGlyphs);
    REQUIRE(tile.lines.size() == 2);
    CHECK(tile.lines[0].text == "alpha");
    CHECK(tile.lines[1].text == "beta");
    CHECK(tile.lines[1].fontSize == 20);
    const auto notes = LayoutText("extraordinarily a note", {.fontSize = 26, .maxWidth = 234}, MeasureGlyphs);
    REQUIRE(notes.lines.size() == 2);
    CHECK(notes.lines[0].text == "extraordinarily");
    CHECK(notes.lines[0].fontSize == 26);
    CHECK(notes.lines[0].width == 390);
    CHECK(notes.lines[1].text == "a note");
    const auto hud = LayoutText("abc def ghi jkl mno pqr stu vwx yz",
                                {.fontSize = 19,
                                 .maxWidth = 196,
                                 .maxLines = 3,
                                 .widthOverflow = TextWidthOverflow::ShrinkToFit,
                                 .lineOverflow = TextLineOverflow::AppendToLastLine},
                                MeasureGlyphs);
    REQUIRE(hud.lines.size() == 3);
    CHECK(hud.lines[0].text == "abc def");
    CHECK(hud.lines[1].text == "ghi jkl");
    CHECK(hud.lines[2].text == "mno pqr stu vwx yz");
    CHECK(hud.lines[2].fontSize == 10);
    CHECK(hud.lines[2].width == 180);
}

TEST_CASE("Engine.TextLayout.UnboundedWidthAndSingleLineLimits", "[engine][graphics]") {
    const auto unlimited = LayoutText("one two three four", {.fontSize = 10}, MeasureGlyphs);
    REQUIRE(unlimited.lines.size() == 1);
    CHECK(unlimited.lines[0].text == "one two three four");
    const auto discard = LayoutText("one two three", {.fontSize = 10, .maxWidth = 30, .maxLines = 1}, MeasureGlyphs);
    REQUIRE(discard.lines.size() == 1);
    CHECK(discard.lines[0].text == "one");
    const auto append =
        LayoutText("one two three",
                   {.fontSize = 10, .maxWidth = 30, .maxLines = 1, .lineOverflow = TextLineOverflow::AppendToLastLine},
                   MeasureGlyphs);
    REQUIRE(append.lines.size() == 1);
    CHECK(append.lines[0].text == "one two three");
}

TEST_CASE("Engine.TextLayout.FontFittingCorrectsRoundingAndRetainsMinimumSize", "[engine][graphics]") {
    const TextMeasure rounded = [](const char*, int size) { return size == 6 ? 65 : size * 10; };
    CHECK(FittedTextSize("text", 10, 60, rounded) == 5);
    CHECK(FittedTextSize("text", 10, 1, rounded) == 1);
    CHECK(FittedTextSize("text", 10, 0, rounded) == 10);
    CHECK(FittedTextSize("", 10, 10, MeasureGlyphs) == 10);
    const auto fitted =
        LayoutText("text", {.fontSize = 10, .maxWidth = 60, .widthOverflow = TextWidthOverflow::ShrinkToFit}, rounded);
    REQUIRE(fitted.lines.size() == 1);
    CHECK(fitted.lines[0].fontSize == 5);
    CHECK(fitted.lines[0].width == 50);
    CHECK_THROWS_AS(LayoutText("text", {.fontSize = 0}, MeasureGlyphs), std::invalid_argument);
    CHECK_THROWS_AS(LayoutText("text", {.maxWidth = -1}, MeasureGlyphs), std::invalid_argument);
    CHECK_THROWS_AS(LayoutText("text", {}, {}), std::invalid_argument);
}
