#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

class FontData;
struct kbts_shape_context;

// One shaped glyph. Lengths are in units of the first font's height (ascent to descent), so one shaping serves every
// size; fallback fonts are scaled to the first font's em.
struct ShapedGlyph {
    int index = 0;           // Glyph index in its font
    std::uint32_t font = 0;  // Index into the shaper's fonts
    std::uint32_t unit = 0;  // The break unit it belongs to
    float advance = 0.0f;    // Pen movement after it, kerning included
    float x = 0.0f;          // Offset from the pen
    float y = 0.0f;          // Offset from the baseline, down positive
    bool space = false;      // Moves the pen only; never drawn
};
// Glyphs of one font, script and direction, in the order they are drawn.
struct ShapedRun {
    std::uint32_t first = 0;
    std::uint32_t count = 0;
    std::uint8_t level = 0; // Bidirectional embedding level: even left to right, odd right to left
};
// A grapheme: what a line can be broken between when a word does not fit.
struct ShapedUnit {
    float advance = 0.0f;        // Its glyphs' advances
    std::uint32_t spaced = 0;    // Glyphs letter spacing applies after (those that move the pen)
    bool space = false;          // Whitespace: may hang past the line's end
    bool breakBefore = false;    // A line may start here (Unicode line breaking)
};
// One line of the text as written, up to a '\n'. Runs are in logical order; units too.
struct ShapedParagraph {
    std::vector<ShapedGlyph> glyphs;
    std::vector<ShapedRun> runs;
    std::vector<ShapedUnit> units;
};

// Shapes text with a font and its fallbacks (kb_text_shape): Unicode segmentation, OpenType substitution and positioning,
// and left-to-right / right-to-left runs. A character takes the first font that has it; one no font has becomes the
// first font's '?'. Tabs are four spaces wide.
class TextShaper {
public:
    explicit TextShaper(std::vector<const FontData*> fonts);
    ~TextShaper();

    // 'ligatures' false leaves optional ligatures out, as letter spacing asks.
    [[nodiscard]] std::vector<ShapedParagraph> Shape(std::string_view text, bool ligatures) const;
    [[nodiscard]] std::span<const FontData* const> Fonts() const { return _fonts; }

private:
    void ShapeParagraph(std::span<const char32_t> codepoints, bool ligatures, ShapedParagraph& paragraph) const;

    std::vector<const FontData*> _fonts;
    mutable std::mutex _mutex; // One context, used by one call at a time
    mutable kbts_shape_context* _context = nullptr; // Replaced after an error

public:
    TextShaper(const TextShaper&) = delete;
    TextShaper& operator=(const TextShaper&) = delete;
};
