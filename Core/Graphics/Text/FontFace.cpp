#include "Graphics/Text/FontFace.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
#include "Graphics/Text/Internals/FontData.h"
#include "Graphics/Text/Internals/TextShaper.h"

namespace {
float LineHeightOf(const FontData& font, float size) {
    return (font.Ascent() - font.Descent() + font.LineGap()) * font.Scale(size);
}

// A unit's width at one size: its glyphs and the letter spacing after each glyph that moves the pen. 'visible' leaves
// out the spacing after its last glyph, which only matters when something follows.
struct UnitWidth {
    float full = 0.0f;
    float visible = 0.0f;
};
UnitWidth WidthOf(const ShapedUnit& unit, float size, float letterSpacing) {
    const float glyphs = unit.advance * size;
    const float spacing = static_cast<float>(unit.spaced) * letterSpacing;
    return {glyphs + spacing, glyphs + (unit.spaced > 0 ? spacing - letterSpacing : 0.0f)};
}

// Widest paragraph, or widest piece no line may break inside when 'words' (its spaces left out). Letter spacing after
// the last glyph is included, as the callers expect.
float Widest(const std::vector<ShapedParagraph>& paragraphs, float size, float letterSpacing, bool words) {
    float widest = 0.0f;
    for (const ShapedParagraph& paragraph : paragraphs) {
        float run = 0.0f;
        for (const ShapedUnit& unit : paragraph.units) {
            if (words && unit.breakBefore) {
                widest = std::max(widest, run);
                run = 0.0f;
            }
            if (!(words && unit.space)) run += WidthOf(unit, size, letterSpacing).full;
        }
        widest = std::max(widest, run);
    }
    return widest;
}

// Units [begin, visibleEnd) of one paragraph drawn on a line. Spaces after visibleEnd hang past the line's end.
struct LineSpan {
    std::uint32_t begin = 0;
    std::uint32_t visibleEnd = 0;
    float width = 0.0f;
};

// Greedy line breaking: a line ends at the last place Unicode allows before the unit that does not fit or, when one
// word is longer than the line, between graphemes.
std::vector<LineSpan> BreakLines(const ShapedParagraph& paragraph, float size, float letterSpacing, float maxWidth,
                                 bool wrap) {
    std::vector<LineSpan> lines;
    const auto count = static_cast<std::uint32_t>(paragraph.units.size());
    std::uint32_t lineStart = 0;
    std::uint32_t lastVisible = 0; // One past the last visible unit on the line
    float pen = 0.0f;              // Where the next unit starts
    float visibleEnd = 0.0f;       // Right end of the last visible unit
    bool visible = false;
    bool canBreak = false;
    std::uint32_t breakAt = 0; // The last unit a line may start at, and the line up to it
    std::uint32_t breakVisible = 0;
    float breakWidth = 0.0f;
    float breakPen = 0.0f;
    for (std::uint32_t i = 0; i < count; ++i) {
        const ShapedUnit& unit = paragraph.units[i];
        const UnitWidth width = WidthOf(unit, size, letterSpacing);
        if (unit.breakBefore && visible) {
            canBreak = true;
            breakAt = i;
            breakVisible = lastVisible;
            breakWidth = visibleEnd;
            breakPen = pen;
        }
        if (unit.space) {
            pen += width.full;
            continue;
        }
        if (wrap && visible && pen + width.visible > maxWidth) {
            if (canBreak) { // Move what follows the last break to a new line
                lines.push_back({lineStart, breakVisible, std::max(breakWidth, 0.0f)});
                lineStart = breakAt;
                pen -= breakPen;
                visible = lastVisible > breakAt;
                visibleEnd = visible ? visibleEnd - breakPen : 0.0f;
                canBreak = false;
            } else { // A word longer than the line: break between graphemes
                lines.push_back({lineStart, lastVisible, std::max(visibleEnd, 0.0f)});
                lineStart = i;
                pen = 0.0f;
                visibleEnd = 0.0f;
                visible = false;
            }
        }
        visibleEnd = pen + width.visible;
        pen += width.full;
        visible = true;
        lastVisible = i + 1;
    }
    lines.push_back({lineStart, visible ? lastVisible : lineStart, visible ? std::max(visibleEnd, 0.0f) : 0.0f});
    return lines;
}

// One run's glyphs on a line.
struct LinePiece {
    std::uint32_t run = 0;
    std::uint8_t level = 0;
};

// Places one line's glyphs left to right. Runs go in visual order by Unicode rule L2 (from the highest level down to
// the lowest odd one, every sequence at that level or higher is reversed); each run's glyphs are already in drawn order.
void PlaceLine(const ShapedParagraph& paragraph, const LineSpan& span, float size, float letterSpacing,
               std::vector<TextBlock::Glyph>& glyphs) {
    const auto onLine = [&](const ShapedGlyph& glyph) {
        return glyph.unit >= span.begin && glyph.unit < span.visibleEnd;
    };
    std::vector<LinePiece> pieces;
    int highest = 0;
    int lowestOdd = 255;
    for (std::uint32_t r = 0; r < paragraph.runs.size(); ++r) {
        const ShapedRun& run = paragraph.runs[r];
        const auto begin = paragraph.glyphs.begin() + run.first;
        if (std::none_of(begin, begin + run.count, onLine)) continue;
        pieces.push_back({r, run.level});
        highest = std::max(highest, static_cast<int>(run.level));
        if (run.level % 2 == 1) lowestOdd = std::min(lowestOdd, static_cast<int>(run.level));
    }
    for (int level = highest; level >= lowestOdd; --level) {
        for (std::size_t i = 0; i < pieces.size();) {
            if (pieces[i].level < level) {
                ++i;
                continue;
            }
            std::size_t end = i;
            while (end < pieces.size() && pieces[end].level >= level)
                ++end;
            std::reverse(pieces.begin() + static_cast<std::ptrdiff_t>(i),
                         pieces.begin() + static_cast<std::ptrdiff_t>(end));
            i = end;
        }
    }
    float pen = 0.0f;
    for (const LinePiece& piece : pieces) {
        const ShapedRun& run = paragraph.runs[piece.run];
        for (std::uint32_t g = run.first; g < run.first + run.count; ++g) {
            const ShapedGlyph& glyph = paragraph.glyphs[g];
            if (!onLine(glyph)) continue;
            if (!glyph.space) glyphs.push_back({glyph.index, pen + glyph.x * size, glyph.y * size, glyph.font});
            pen += glyph.advance * size + (glyph.advance != 0.0f ? letterSpacing : 0.0f);
        }
    }
}
} // namespace

FontFace::FontFace(std::unique_ptr<FontData> data)
    : _data(std::move(data))
    , _shaper(std::make_unique<TextShaper>(std::vector<const FontData*>{_data.get()})) {}

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

std::vector<ShapedParagraph> FontFace::Shape(std::string_view text, float letterSpacing) const {
    return _shaper->Shape(text, letterSpacing == 0.0f); // Letter spacing pulls ligatures apart, so it turns them off
}

Float2 FontFace::Measure(std::string_view text, float size, float letterSpacing) const {
    if (!(size > 0.0f) || !std::isfinite(size)) return {};
    const auto paragraphs = Shape(text, letterSpacing);
    const float width = Widest(paragraphs, size, letterSpacing, false);
    return {width > 0.0f ? width - letterSpacing : 0.0f,
            LineHeightOf(*_data, size) * static_cast<float>(paragraphs.size() - 1) + size};
}

float FontFace::FittedSize(std::string_view text, float size, float maxWidth, float letterSpacing) const {
    if (!(size > 0.0f) || !std::isfinite(size) || !(maxWidth > 0.0f)) return size;
    const float widest = Widest(Shape(text, letterSpacing), size, letterSpacing, false) - letterSpacing;
    return widest > maxWidth ? size * maxWidth / widest : size;
}

TextBlock FontFace::Layout(std::string_view text, const TextStyle& style) const {
    TextBlock block;
    block._font = _data->Id();
    float size = style.size;
    if (!(size > 0.0f) || !std::isfinite(size)) return block;
    const bool wrap = style.wrap == TextWrap::Word && style.maxWidth > 0.0f;
    const auto paragraphs = Shape(text, style.letterSpacing);
    if (style.fit == TextFit::Shrink && style.maxWidth > 0.0f) {
        const float widest = Widest(paragraphs, size, style.letterSpacing, wrap) - style.letterSpacing;
        if (widest > style.maxWidth) size *= style.maxWidth / widest;
    }
    const float lineHeight = style.lineHeight > 0.0f ? style.lineHeight * size / style.size
                                                     : LineHeightOf(*_data, size) * std::max(style.lineSpacing, 0.0f);
    float top = 0.0f;
    for (const ShapedParagraph& paragraph : paragraphs) {
        for (const LineSpan& span : BreakLines(paragraph, size, style.letterSpacing, style.maxWidth, wrap)) {
            const auto first = static_cast<std::uint32_t>(block._glyphs.size());
            PlaceLine(paragraph, span, size, style.letterSpacing, block._glyphs);
            block._lines.push_back({first, static_cast<std::uint32_t>(block._glyphs.size()) - first, span.width, top});
            top += lineHeight;
        }
    }
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

bool FontFace::HasCharacter(char32_t codepoint) const {
    return _data->Has(codepoint) || std::any_of(_fallbacks.begin(), _fallbacks.end(), [&](const FontFace& fallback) {
               return fallback._data->Has(codepoint);
           });
}

void FontFace::AddFallback(FontFace fallback) {
    if (!fallback._data) throw std::invalid_argument("FontFace::AddFallback: the fallback was moved from");
    std::vector<FontFace> nested = std::move(fallback._fallbacks);
    fallback._fallbacks.clear();
    fallback._shaper.reset();
    _fallbacks.push_back(std::move(fallback));
    for (FontFace& font : nested)
        _fallbacks.push_back(std::move(font));
    std::vector<const FontData*> fonts{_data.get()};
    for (const FontFace& font : _fallbacks)
        fonts.push_back(font._data.get());
    _shaper = std::make_unique<TextShaper>(std::move(fonts));
}

std::uint32_t FontFace::Id() const { return _data->Id(); }
