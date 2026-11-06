#pragma once
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>
#include "Graphics/Text/FontFace.h"

// A rasterized glyph: coverage (or distance) bytes, rows top to bottom, placed relative to the pen on the baseline.
struct GlyphRaster {
    std::vector<std::uint8_t> pixels;
    int width = 0;
    int height = 0;
    int left = 0; // Bitmap's left edge from the pen, pixels
    int top = 0;  // Bitmap's top edge from the baseline (negative: above), pixels
};

struct kbts_font;

// Parsed font file behind a FontFace: stb_truetype rasterizes and measures, kb_text_shape shapes (TextShaper). Sizes
// follow the font's ascent-to-descent height.
class FontData {
public:
    // Throws runtime_error when 'bytes' is not a usable font.
    explicit FontData(std::vector<unsigned char> bytes);
    ~FontData();

    // Glyph index of 'codepoint', or of '?' when the font lacks it.
    [[nodiscard]] int GlyphIndex(char32_t codepoint) const;
    [[nodiscard]] bool Has(char32_t codepoint) const;
    // Font units.
    [[nodiscard]] float Advance(int glyph) const;
    [[nodiscard]] float Ascent() const { return _ascent; }
    [[nodiscard]] float Descent() const { return _descent; } // Negative
    [[nodiscard]] float LineGap() const { return _lineGap; }
    // Logical units per font unit at 'size'.
    [[nodiscard]] float Scale(float size) const { return size / (_ascent - _descent); }
    // Ascent-to-descent height in ems. Fallback fonts are drawn with the em of the font they stand in for.
    [[nodiscard]] float EmHeight() const { return _emHeight; }
    [[nodiscard]] std::uint32_t Id() const { return _id; }
    // The '?' glyph drawn for characters no font has.
    [[nodiscard]] int Fallback() const { return _fallback; }
    // The font as kb_text_shape reads it; for TextShaper only.
    [[nodiscard]] kbts_font& Shaping() const;

    [[nodiscard]] GlyphRaster Rasterize(int glyph, float pixelSize) const;
    // Signed distance field: 'padding' pixels around the glyph; a byte is 128 on the outline and changes by
    // 'distanceScale' per pixel (larger inside).
    [[nodiscard]] GlyphRaster RasterizeDistance(int glyph, float pixelSize, int padding, float distanceScale) const;

private:
    struct Info;
    std::vector<unsigned char> _bytes;
    std::unique_ptr<Info> _info;
    std::vector<int> _ascii; // Glyph index per ASCII codepoint
    int _fallback = 0;
    float _ascent = 0.0f;
    float _descent = 0.0f;
    float _lineGap = 0.0f;
    float _emHeight = 1.0f;
    std::uint32_t _id;

public:
    FontData(const FontData&) = delete;
    FontData& operator=(const FontData&) = delete;
};

struct FontAccess {
    [[nodiscard]] static const FontData& Data(const FontFace& font) { return *font._data; }
    // The font a block's glyph names: 0 the face, then its fallbacks.
    [[nodiscard]] static const FontData& Data(const FontFace& font, std::uint32_t index) {
        return index == 0 || index > font._fallbacks.size() ? *font._data : *font._fallbacks[index - 1]._data;
    }
    // The size font 'index' is drawn at, per unit of the block's size: 1 for the face, its em over the face's em for a
    // fallback.
    [[nodiscard]] static float SizeScale(const FontFace& font, std::uint32_t index) {
        return Data(font, index).EmHeight() / font._data->EmHeight();
    }
};
