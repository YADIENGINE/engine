#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include "Graphics/Text/TextBlock.h"
#include "Graphics/Text/TextStyle.h"
#include "Utilities/Float2.h"

class FontData;

// A TrueType / OpenType (glyf outlines) font. Glyphs are rasterized on demand for the pixel size they cover on
// screen, so every size is sharp and any character the font has can be drawn. Headless: measuring and layout work
// without a window. Text is UTF-8; characters missing from the font draw as '?'.
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
    [[nodiscard]] bool HasCharacter(char32_t codepoint) const;
    // Unique for the program's lifetime (never reused by another font).
    [[nodiscard]] std::uint32_t Id() const;

private:
    friend struct FontAccess;
    explicit FontFace(std::unique_ptr<FontData> data);
    std::unique_ptr<FontData> _data;

public:
    FontFace(FontFace&&) noexcept;
    FontFace& operator=(FontFace&&) noexcept;
    FontFace(const FontFace&) = delete;
    FontFace& operator=(const FontFace&) = delete;
};
