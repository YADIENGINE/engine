#include "Graphics/Text/Internals/TextShaper.h"
#include <kb_text_shape.h>
#include <new>
#include <stdexcept>
#include <utility>
#include "Graphics/Text/Internals/FontData.h"
#include "Utilities/Utf8.h"

namespace {
constexpr float TabSpaces = 4.0f;

bool Space(char32_t codepoint) { return codepoint == U' ' || codepoint == U'\t' || codepoint == 0x3000; }

// Earlier fonts win, so the face comes first and its fallbacks follow in the order they were added.
kbts_shape_context* MakeContext(std::span<const FontData* const> fonts) {
    kbts_shape_context* context =
        kbts_CreateShapeContext2(nullptr, nullptr, KBTS_SHAPE_CONTEXT_FLAG_FONT_PRIORITY_BOTTOM_TO_TOP);
    if (!context) throw std::bad_alloc();
    for (const FontData* font : fonts) {
        if (!kbts_ShapePushFont(context, &font->Shaping())) {
            kbts_DestroyShapeContext(context);
            throw std::bad_alloc();
        }
    }
    return context;
}

// Embedding levels from the run's and the paragraph's direction (the two levels kb_text_shape resolves).
std::uint8_t Level(const kbts_run& run) {
    const bool rightToLeft = run.Direction == KBTS_DIRECTION_RTL;
    if (run.ParagraphDirection == KBTS_DIRECTION_RTL) return rightToLeft ? 1 : 2;
    return rightToLeft ? 1 : 0;
}
} // namespace

TextShaper::TextShaper(std::vector<const FontData*> fonts)
    : _fonts(std::move(fonts))
    , _context(MakeContext(_fonts)) {}

TextShaper::~TextShaper() { kbts_DestroyShapeContext(_context); }

std::vector<ShapedParagraph> TextShaper::Shape(std::string_view text, bool ligatures) const {
    std::vector<ShapedParagraph> paragraphs(1);
    std::vector<char32_t> codepoints;
    const std::lock_guard lock(_mutex);
    for (std::size_t at = 0; at < text.size();) {
        const char32_t codepoint = NextCodepoint(text, at);
        if (codepoint == U'\r') continue;
        if (codepoint == U'\n') {
            ShapeParagraph(codepoints, ligatures, paragraphs.back());
            codepoints.clear();
            paragraphs.emplace_back();
            continue;
        }
        codepoints.push_back(codepoint);
    }
    ShapeParagraph(codepoints, ligatures, paragraphs.back());
    return paragraphs;
}

void TextShaper::ShapeParagraph(std::span<const char32_t> codepoints, bool ligatures,
                                ShapedParagraph& paragraph) const {
    if (codepoints.empty()) return;
    kbts_ShapeBegin(_context, KBTS_DIRECTION_DONT_KNOW, KBTS_LANGUAGE_DONT_KNOW);
    if (!ligatures) {
        kbts_ShapePushFeature(_context, KBTS_FEATURE_TAG_liga, 0);
        kbts_ShapePushFeature(_context, KBTS_FEATURE_TAG_clig, 0);
    }
    for (const char32_t codepoint : codepoints)
        kbts_ShapeCodepoint(_context, static_cast<int>(codepoint == U'\t' ? U' ' : codepoint));
    if (!ligatures) {
        (void)kbts_ShapePopFeature(_context, KBTS_FEATURE_TAG_clig);
        (void)kbts_ShapePopFeature(_context, KBTS_FEATURE_TAG_liga);
    }
    kbts_ShapeEnd(_context);

    // Units are graphemes; a line may start at one that Unicode line breaking allows.
    std::vector<std::uint32_t> unitOf(codepoints.size(), 0);
    kbts_shape_codepoint_iterator codepointIt = kbts_ShapeCurrentCodepointsIterator(_context);
    kbts_shape_codepoint shaped{};
    int index = 0;
    while (kbts_ShapeCodepointIteratorNext(&codepointIt, &shaped, &index)) {
        if (index < 0 || static_cast<std::size_t>(index) >= codepoints.size()) continue;
        if (paragraph.units.empty() || (shaped.BreakFlags & KBTS_BREAK_FLAG_GRAPHEME)) {
            paragraph.units.push_back({.space = Space(codepoints[static_cast<std::size_t>(index)]),
                                       .breakBefore = index > 0 && (shaped.BreakFlags & KBTS_BREAK_FLAG_LINE_SOFT)});
        }
        unitOf[static_cast<std::size_t>(index)] = static_cast<std::uint32_t>(paragraph.units.size() - 1);
    }

    const FontData& first = *_fonts.front();
    const float firstHeight = first.Ascent() - first.Descent();
    const float missingAdvance = first.Advance(first.Fallback()) / firstHeight;
    kbts_run run{};
    while (kbts_ShapeRun(_context, &run)) {
        std::uint32_t font = 0;
        while (font + 1 < _fonts.size() && &_fonts[font]->Shaping() != run.Font)
            ++font;
        // In units of the first font's height; a fallback keeps its em equal to the first font's em.
        const FontData& data = *_fonts[font];
        const float height = (data.Ascent() - data.Descent()) * first.EmHeight() / data.EmHeight();
        ShapedRun shapedRun{.first = static_cast<std::uint32_t>(paragraph.glyphs.size()), .level = Level(run)};
        kbts_glyph* glyph = nullptr;
        while (kbts_GlyphIteratorNext(&run.Glyphs, &glyph)) {
            const auto source = static_cast<std::size_t>(glyph->UserIdOrCodepointIndex);
            if (source >= codepoints.size()) continue;
            ShapedGlyph placed{.unit = unitOf[source], .space = Space(codepoints[source])};
            if (glyph->Id == 0) {           // No font has it
                if (glyph->AdvanceX == 0) continue; // An invisible control character
                placed.index = first.Fallback();
                placed.advance = missingAdvance;
            } else {
                placed.index = glyph->Id;
                placed.font = font;
                placed.advance = static_cast<float>(glyph->AdvanceX) / height;
                placed.x = static_cast<float>(glyph->OffsetX) / height;
                placed.y = -static_cast<float>(glyph->OffsetY) / height;
            }
            if (codepoints[source] == U'\t') placed.advance *= TabSpaces;
            ShapedUnit& unit = paragraph.units[placed.unit];
            unit.advance += placed.advance;
            if (placed.advance != 0.0f) ++unit.spaced;
            paragraph.glyphs.push_back(placed);
        }
        shapedRun.count = static_cast<std::uint32_t>(paragraph.glyphs.size()) - shapedRun.first;
        if (shapedRun.count > 0) paragraph.runs.push_back(shapedRun);
    }
    if (kbts_ShapeError(_context) != KBTS_SHAPE_ERROR_NONE) {
        // A context with an error does nothing more; start a clean one for the next text.
        kbts_DestroyShapeContext(_context);
        _context = nullptr;
        _context = MakeContext(_fonts);
        throw std::runtime_error("Text shaping ran out of memory");
    }
}
