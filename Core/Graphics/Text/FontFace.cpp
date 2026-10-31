#include "Graphics/Text/FontFace.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
#include "Graphics/Text/Internals/FontData.h"
#include "Utilities/Utf8.h"

namespace {
constexpr float TabSpaces = 4.0f;

bool Space(char32_t codepoint) { return codepoint == U' ' || codepoint == U'\t' || codepoint == 0x3000; }

// Lays glyphs into lines at one size. Spaces are not stored; they only move the pen and mark where a line may wrap.
class LineBuilder {
public:
    LineBuilder(const FontData& font, float size, float letterSpacing, float lineHeight, float maxWidth, bool wrap,
                std::vector<TextBlock::Glyph>& glyphs, std::vector<TextBlock::Line>& lines)
        : _font(font)
        , _scale(font.Scale(size))
        , _letterSpacing(letterSpacing)
        , _lineHeight(lineHeight)
        , _maxWidth(maxWidth)
        , _wrap(wrap && maxWidth > 0.0f)
        , _glyphs(glyphs)
        , _lines(lines) {}

    void Add(char32_t codepoint) {
        if (codepoint == U'\r') return;
        if (codepoint == U'\n') {
            EndLine(Count(), _visibleEnd);
            StartLine(Count());
            return;
        }
        const int glyph = _font.GlyphIndex(codepoint == U'\t' ? U' ' : codepoint);
        const float kerning = _previous >= 0 ? _font.Kerning(_previous, glyph) * _scale : 0.0f;
        const float advance = _font.Advance(glyph) * _scale * (codepoint == U'\t' ? TabSpaces : 1.0f);
        _previous = glyph;
        if (Space(codepoint)) {
            if (!_inSpace && Count() > _lineStart) {
                _breakIndex = Count();
                _breakWidth = _visibleEnd;
                _canBreak = true;
            }
            _inSpace = true;
            _pen += kerning + advance + _letterSpacing;
            _resume = _pen;
            return;
        }
        _inSpace = false;
        float x = _pen + kerning;
        if (_wrap && x + advance > _maxWidth && Count() > _lineStart) {
            if (_canBreak) { // Move the words after the last space run to a new line
                const float shift = _resume;
                EndLine(_breakIndex, _breakWidth);
                for (std::size_t i = _breakIndex; i < _glyphs.size(); ++i)
                    _glyphs[i].x -= shift;
                _lineStart = _breakIndex;
                _pen -= shift;
                _visibleEnd -= shift;
                _canBreak = false;
                x -= shift;
            } else { // A word longer than the line: break between characters
                EndLine(Count(), _visibleEnd);
                StartLine(Count());
                x = 0.0f;
            }
        }
        _glyphs.push_back({glyph, x});
        _visibleEnd = x + advance;
        _pen = x + advance + _letterSpacing;
    }

    void Finish() { EndLine(Count(), _visibleEnd); }

private:
    [[nodiscard]] std::uint32_t Count() const { return static_cast<std::uint32_t>(_glyphs.size()); }
    void EndLine(std::uint32_t end, float width) {
        _lines.push_back({_lineStart, end - _lineStart, std::max(width, 0.0f), _top});
        _top += _lineHeight;
    }
    void StartLine(std::uint32_t start) {
        _lineStart = start;
        _pen = 0.0f;
        _visibleEnd = 0.0f;
        _previous = -1;
        _canBreak = false;
        _inSpace = false;
    }

    const FontData& _font;
    float _scale;
    float _letterSpacing;
    float _lineHeight;
    float _maxWidth;
    bool _wrap;
    std::vector<TextBlock::Glyph>& _glyphs;
    std::vector<TextBlock::Line>& _lines;
    float _pen = 0.0f;        // Where the next character starts
    float _visibleEnd = 0.0f; // Right end of the last visible glyph on the line
    float _top = 0.0f;
    std::uint32_t _lineStart = 0;
    std::uint32_t _breakIndex = 0; // Glyphs before the last space run on this line
    float _breakWidth = 0.0f;      // Line width up to that space run
    float _resume = 0.0f;          // Pen after that space run
    bool _canBreak = false;
    bool _inSpace = false;
    int _previous = -1;
};

float LineHeightOf(const FontData& font, float size) {
    return (font.Ascent() - font.Descent() + font.LineGap()) * font.Scale(size);
}

// Widest unwrapped line, or widest word when 'words'.
float Widest(const FontData& font, std::string_view text, float size, float letterSpacing, bool words) {
    const float scale = font.Scale(size);
    float widest = 0.0f;
    float run = 0.0f;
    int previous = -1;
    for (std::size_t at = 0; at < text.size();) {
        const char32_t codepoint = NextCodepoint(text, at);
        if (codepoint == U'\n' || (words && Space(codepoint))) {
            widest = std::max(widest, run);
            run = 0.0f;
            previous = -1;
            if (codepoint == U'\n' || words) continue;
        }
        if (codepoint == U'\r') continue;
        const int glyph = font.GlyphIndex(codepoint == U'\t' ? U' ' : codepoint);
        if (previous >= 0) run += font.Kerning(previous, glyph) * scale;
        run += font.Advance(glyph) * scale * (codepoint == U'\t' ? TabSpaces : 1) + letterSpacing;
        previous = glyph;
    }
    return std::max(widest, run);
}
} // namespace

FontFace::FontFace(std::unique_ptr<FontData> data)
    : _data(std::move(data)) {}

FontFace::~FontFace() = default;
FontFace::FontFace(FontFace&&) noexcept = default;
FontFace& FontFace::operator=(FontFace&&) noexcept = default;

FontFace FontFace::FromFile(const std::filesystem::path& file) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) throw std::runtime_error("Could not read font: " + file.string());
    std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    try {
        return FontFace(std::make_unique<FontData>(std::move(bytes)));
    } catch (const std::runtime_error& error) {
        throw std::runtime_error(file.string() + ": " + error.what());
    }
}

FontFace FontFace::FromMemory(std::span<const std::byte> data) {
    std::vector<unsigned char> bytes(data.size());
    std::transform(data.begin(), data.end(), bytes.begin(), [](std::byte b) { return static_cast<unsigned char>(b); });
    return FontFace(std::make_unique<FontData>(std::move(bytes)));
}

Float2 FontFace::Measure(std::string_view text, float size, float letterSpacing) const {
    if (!(size > 0.0f) || !std::isfinite(size)) return {};
    std::size_t lines = 1;
    for (const char c : text)
        lines += c == '\n' ? 1 : 0;
    const float width = Widest(*_data, text, size, letterSpacing, false);
    return {width > 0.0f ? width - letterSpacing : 0.0f,
            LineHeightOf(*_data, size) * static_cast<float>(lines - 1) + size};
}

float FontFace::FittedSize(std::string_view text, float size, float maxWidth, float letterSpacing) const {
    if (!(size > 0.0f) || !std::isfinite(size) || !(maxWidth > 0.0f)) return size;
    const float widest = Widest(*_data, text, size, letterSpacing, false) - letterSpacing;
    return widest > maxWidth ? size * maxWidth / widest : size;
}

TextBlock FontFace::Layout(std::string_view text, const TextStyle& style) const {
    TextBlock block;
    block._font = _data->Id();
    float size = style.size;
    if (!(size > 0.0f) || !std::isfinite(size)) return block;
    const bool wrap = style.wrap == TextWrap::Word;
    if (style.fit == TextFit::Shrink && style.maxWidth > 0.0f) {
        const float widest = Widest(*_data, text, size, style.letterSpacing, wrap) - style.letterSpacing;
        if (widest > style.maxWidth) size *= style.maxWidth / widest;
    }
    const float lineHeight = style.lineHeight > 0.0f ? style.lineHeight * size / style.size
                                                     : LineHeightOf(*_data, size) * std::max(style.lineSpacing, 0.0f);
    LineBuilder builder(*_data, size, style.letterSpacing, lineHeight, style.maxWidth, wrap, block._glyphs,
                        block._lines);
    for (std::size_t at = 0; at < text.size();)
        builder.Add(NextCodepoint(text, at));
    builder.Finish();
    block._size = size;
    block._ascent = _data->Ascent() * _data->Scale(size);
    float width = 0.0f;
    for (const TextBlock::Line& line : block._lines)
        width = std::max(width, line.width);
    block._extent = {width, block._lines.back().top + size};
    return block;
}

float FontFace::LineHeight(float size) const { return LineHeightOf(*_data, size); }

float FontFace::Ascent(float size) const { return _data->Ascent() * _data->Scale(size); }

bool FontFace::HasCharacter(char32_t codepoint) const { return _data->Has(codepoint); }

std::uint32_t FontFace::Id() const { return _data->Id(); }
