#pragma once
#include "Graphics/Paint/PaintStyle.h"
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"

// Where the drawing position sits on each line: its left end, its centre or its right end.
enum class TextAlign { Left, Center, Right };
// None: lines break only at '\n'; Word: also between words (long words between characters) to stay within maxWidth.
enum class TextWrap { None, Word };
// None: text may exceed maxWidth; Shrink: the size goes down until the widest line fits (never up), keeping the
// vertical centre of a line at the requested size (Painter::Text / TextBox with a string).
enum class TextFit { None, Shrink };

struct TextShadow {
    Float2 offset{};
    Color4 color{0, 0, 0, 0}; // Alpha 0: no shadow
};
// A band around the glyphs (drawn under them).
struct TextOutline {
    float width = 0.0f; // 0: no outline
    Color4 color{0, 0, 0, 255};
};

// Layout and look of a text. The drawing position is the top of the first line; horizontally see TextAlign.
struct TextStyle {
    float size = 16.0f; // Font size in logical units (the font's ascent-to-descent height)
    Color4 color{255, 255, 255, 255};
    TextAlign align = TextAlign::Left;
    float maxWidth = 0.0f; // 0: unlimited
    TextWrap wrap = TextWrap::None;
    TextFit fit = TextFit::None;
    float lineSpacing = 1.0f;   // Multiplies the font's line height
    float lineHeight = 0.0f;    // > 0: distance between lines in logical units instead (scaled with a shrink)
    float letterSpacing = 0.0f; // Extra advance after each character, logical units
    GlowStyle glow{};           // Soft light around the glyphs (radius at most half the size)
    TextOutline outline{};
    TextShadow shadow{};
    float emission = 0.0f;
};

// Shorthands for the usual label: shrunk to 'maxWidth' when one is given (0: no limit).
[[nodiscard]] constexpr TextStyle FittedText(float size, Color4 color, float maxWidth = 0.0f,
                                             TextAlign align = TextAlign::Left) {
    return {.size = size, .color = color, .align = align, .maxWidth = maxWidth, .fit = TextFit::Shrink};
}
[[nodiscard]] constexpr TextStyle CenteredText(float size, Color4 color, float maxWidth = 0.0f) {
    return FittedText(size, color, maxWidth, TextAlign::Center);
}
