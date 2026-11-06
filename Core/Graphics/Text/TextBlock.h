#pragma once
#include <cstdint>
#include <vector>
#include "Graphics/Text/TextStyle.h"
#include "Utilities/Geometry/Float2.h"

// A laid-out text (FontFace::Layout): glyphs placed in lines at the final size. Lay out once and draw it every frame
// with Painter::Text to skip shaping; it stays valid as long as its font lives.
class TextBlock {
public:
    struct Glyph {
        int index = 0;          // Glyph index in its font
        float x = 0.0f;         // Pen position on its line, from the line's left end
        float y = 0.0f;         // From the baseline, down positive: a mark sits above or below its letter
        std::uint32_t font = 0; // 0: the block's font; 1 and up: its fallbacks, in the order they were added
    };
    // Glyphs left to right as drawn, also in right-to-left text.
    struct Line {
        std::uint32_t firstGlyph = 0;
        std::uint32_t glyphCount = 0;
        float width = 0.0f;
        float top = 0.0f; // From the block's top
    };

    [[nodiscard]] float Size() const { return _size; } // After TextFit::Shrink
    [[nodiscard]] Float2 Extent() const { return _extent; }
    [[nodiscard]] const std::vector<Line>& Lines() const { return _lines; }
    [[nodiscard]] const std::vector<Glyph>& Glyphs() const { return _glyphs; }
    [[nodiscard]] std::uint32_t FontId() const { return _font; }
    [[nodiscard]] float Ascent() const { return _ascent; } // Baseline below each line's top

private:
    friend class FontFace;
    std::vector<Glyph> _glyphs;
    std::vector<Line> _lines;
    Float2 _extent;
    float _size = 0.0f;
    float _ascent = 0.0f;
    std::uint32_t _font = 0;
};
