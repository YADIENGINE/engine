#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>
#include "Graphics/Text/TextBlock.h"
#include "Graphics/Text/TextStyle.h"
#include "Utilities/Geometry/Float2.h"

class FontData;
class TextShaper;
struct ShapedParagraph;

// A TrueType / OpenType (glyf outlines) font. Glyphs are rasterized on demand for the pixel size they cover on
// screen, so every size is sharp and any character the font has can be drawn. Headless: measuring and layout work
// without a window. Text is UTF-8 and is shaped: kerning and ligatures from the font, joined Arabic letters, marks
// placed on their letters, right-to-left runs in visual order, and lines broken where Unicode allows (also between
// Chinese or Japanese characters, which need no spaces). Characters neither the font nor its fallbacks have draw as '?'.
// Limits: Docs/Graphics/Text.md#other-scripts.
class FontFace {
public:
    // Throws runtime_error when the file cannot be read or is not a usable font.
    [[nodiscard]] static FontFace FromFile(const std::filesystem::path& file);
    [[nodiscard]] static FontFace FromMemory(std::span<const std::byte> data);
    ~FontFace();

    // Size of 'text' on one line per '\n', without wrapping.
    [[nodiscard]] Float2 Measure(std::string_view text, float size, float letterSpacing = 0.0f) const;
    // The size TextFit::Shrink draws 'text' at (no wrapping): 'size', or smaller so the widest line fits 'maxWidth'.
    [[nodiscard]] float FittedSize(std::string_view text, float size, float maxWidth, float letterSpacing = 0.0f) const;
    // Lines, wrapping and fitting according to 'style' (size, maxWidth, wrap, fit, spacing).
    [[nodiscard]] TextBlock Layout(std::string_view text, const TextStyle& style) const;
    [[nodiscard]] float LineHeight(float size) const;
    [[nodiscard]] float Ascent(float size) const;
    // The font or one of its fallbacks has it.
    [[nodiscard]] bool HasCharacter(char32_t codepoint) const;
    // Characters this font lacks come from 'fallback' (several: the first that has a character wins), at the same em;
    // lines keep this font's height. Add them before the font lays out text: a block laid out earlier keeps its glyphs.
    // For an Arabic menu in a Latin font: font.AddFallback(FontFace::FromFile("Fonts/NotoSansArabic-Regular.ttf")).
    void AddFallback(FontFace fallback);
    // Unique for the program's lifetime (never reused by another font).
    [[nodiscard]] std::uint32_t Id() const;

private:
    friend struct FontAccess;
    explicit FontFace(std::unique_ptr<FontData> data);
    [[nodiscard]] std::vector<ShapedParagraph> Shape(std::string_view text, float letterSpacing) const;
    std::unique_ptr<FontData> _data;
    std::vector<FontFace> _fallbacks; // Flat: a fallback's own fallbacks follow it
    std::unique_ptr<TextShaper> _shaper;

public:
    FontFace(FontFace&&) noexcept;
    FontFace& operator=(FontFace&&) noexcept;
    FontFace(const FontFace&) = delete;
    FontFace& operator=(const FontFace&) = delete;
};
