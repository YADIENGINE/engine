// Painter's text drawing (the rest of Painter is in Painter.cpp).
#include "Graphics/Paint/Painter.h"
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include "Graphics/Paint/Internals/PaintContext.h"
#include "Graphics/Paint/Internals/PaintShared.h"
#include "Graphics/Text/FontFace.h"
#include "Graphics/Text/Internals/FontData.h"
#include "Graphics/Text/Internals/GlyphCache.h"
#include "Graphics/Text/Internals/TextLayoutCache.h"

namespace {
constexpr std::string_view NoFont = "Painter: the game has no font (GameSettings::font)";

float AlignOffset(TextAlign align, float width) {
    switch (align) {
    case TextAlign::Left  : return 0.0f;
    case TextAlign::Center: return -width * 0.5f;
    case TextAlign::Right : return -width;
    }
    return 0.0f;
}

struct GlyphLook {
    PaintKind kind = PaintKind::Glyph;
    PaintMode mode = PaintMode::Fill;
    PaintColor color;
    float shape0 = 0.0f;
    float style0 = 0.0f;
    float style1 = 0.0f;
    std::uint8_t emission = 0;
};

// One glyph quad: 'topLeft' and 'size' in drawing coordinates. With 'snap', the quad moves so its corner sits on a
// device pixel (texels map 1:1 to pixels: crisp edges).
void EmitGlyph(DrawList& list, Float2 topLeft, Float2 size, const CachedGlyph& glyph, const GlyphLook& look,
               bool snap) {
    const Affine2& transform = list.State().transform;
    const std::array<Float2, 4> local{topLeft, topLeft + Float2{size.x, 0.0f}, topLeft + size,
                                      topLeft + Float2{0.0f, size.y}};
    const FloatRect& uv = glyph.uv;
    const std::array<Float2, 4> coords{Float2{uv.x, uv.y}, Float2{uv.Right(), uv.y}, Float2{uv.Right(), uv.Bottom()},
                                       Float2{uv.x, uv.Bottom()}};
    Float2 shift;
    if (snap) {
        const Float2 scale = list.CurrentTarget().PixelScale();
        const Float2 corner = transform.Apply(topLeft);
        shift = {std::round(corner.x * scale.x) / scale.x - corner.x,
                 std::round(corner.y * scale.y) / scale.y - corner.y};
    }
    std::array<PaintVertex, 4> corners{};
    for (std::size_t i = 0; i < corners.size(); ++i) {
        PaintVertex& vertex = corners[i];
        const Float2 position = transform.Apply(local[i]) + shift;
        vertex.x = position.x;
        vertex.y = position.y;
        vertex.u = coords[i].x;
        vertex.v = coords[i].y;
        vertex.shape0 = look.shape0;
        vertex.style0 = look.style0;
        vertex.style1 = look.style1;
        vertex.color = look.color;
        vertex.kind = look.kind;
        vertex.flags = static_cast<std::uint8_t>(look.mode);
        vertex.emission = look.emission;
    }
    const std::array textures{glyph.page};
    list.AddQuad(corners, textures);
}
} // namespace

const FontFace& Painter::Font() const {
    if (!_context.font) throw std::logic_error(std::string(NoFont));
    return *_context.font;
}

bool Painter::HasFont() const { return _context.font != nullptr; }

Float2 Painter::Text(std::string_view text, Float2 position, const TextStyle& style) {
    if (text.empty()) return {};
    if (!HasFont()) {
        _context.list.Reject(NoFont);
        return {};
    }
    return Text(Font(), text, position, style);
}

Float2 Painter::TextBox(std::string_view text, const FloatRect& area, const TextStyle& style) {
    if (text.empty()) return {};
    if (!HasFont()) {
        _context.list.Reject(NoFont);
        return {};
    }
    return TextBox(Font(), text, area, style);
}

Float2 Painter::Text(const FontFace& font, std::string_view text, Float2 position, const TextStyle& style) {
    if (!_context.text) throw std::logic_error("Painter::Text: no text services in this renderer");
    if (text.empty()) return {};
    const TextBlock& block = _context.text->layouts.Get(font, text, style);
    // A shrunk text keeps the vertical centre of a line at the requested size (it stays centred in its slot).
    return Text(font, block, {position.x, position.y + (style.size - block.Size()) * 0.5f}, style);
}

Float2 Painter::TextBox(const FontFace& font, std::string_view text, const FloatRect& area, const TextStyle& style) {
    TextStyle boxed = style;
    boxed.maxWidth = area.width;
    const float x = style.align == TextAlign::Left    ? area.x
                    : style.align == TextAlign::Right ? area.Right()
                                                      : area.Center().x;
    return Text(font, text, {x, area.y}, boxed);
}

Float2 Painter::Text(const FontFace& font, const TextBlock& block, Float2 position, const TextStyle& style) {
    PaintContext& context = _context;
    if (!context.text) throw std::logic_error("Painter::Text: no text services in this renderer");
    DrawList& list = context.list;
    if (block.FontId() != font.Id()) {
        list.Reject("Painter::Text: the block was laid out with another font");
        return block.Extent();
    }
    if (!ValidWidth(list, style.outline.width, "Text outline width must not be negative")) return block.Extent();
    if (block.Glyphs().empty() || !Finite(position)) return block.Extent();
    const FontData& data = FontAccess::Data(font);
    GlyphCache& glyphs = context.text->glyphs;
    const Affine2& transform = list.State().transform;
    const float size = block.Size();
    const float pixels = size * list.PixelScale();
    const int raster = GlyphCache::RasterSize(pixels);
    const float unit = size / static_cast<float>(raster); // Drawing units per raster pixel
    const bool snap = raster == static_cast<int>(std::lround(pixels)) && transform.AxisAligned() &&
                      std::abs(transform.a - transform.d) < 1e-4f;
    const std::uint8_t emission = EmissionByte(style.emission);
    const float distanceUnit = size / static_cast<float>(GlyphCache::DistanceSize);
    const float distanceReach = static_cast<float>(GlyphCache::DistancePadding);

    // Calls 'draw(glyph index, pen on the baseline)' for every glyph.
    const auto each = [&](auto&& draw) {
        for (const TextBlock::Line& line : block.Lines()) {
            const float left = position.x + AlignOffset(style.align, line.width);
            const float baseline = position.y + line.top + block.Ascent();
            for (std::uint32_t i = 0; i < line.glyphCount; ++i) {
                const TextBlock::Glyph& glyph = block.Glyphs()[line.firstGlyph + i];
                draw(glyph.index, Float2{left + glyph.x, baseline});
            }
        }
    };
    const auto coverage = [&](Float2 offset, const GlyphLook& look) {
        each([&](int index, Float2 pen) {
            const CachedGlyph& glyph = glyphs.Coverage(data, index, raster);
            if (glyph.empty) return;
            EmitGlyph(list, pen + offset + Float2{glyph.left, glyph.top} * unit,
                      Float2{glyph.width, glyph.height} * unit, glyph, look, snap);
        });
    };
    const auto distance = [&](const GlyphLook& look) {
        each([&](int index, Float2 pen) {
            const CachedGlyph& glyph = glyphs.Distance(data, index);
            if (glyph.empty) return;
            EmitGlyph(list, pen + Float2{glyph.left, glyph.top} * distanceUnit,
                      Float2{glyph.width, glyph.height} * distanceUnit, glyph, look, false);
        });
    };

    if (style.shadow.color.a > 0)
        coverage(style.shadow.offset, {.color = list.Shade(style.shadow.color), .emission = 0});
    if (const auto glow = EffectiveGlow(list, style.glow, context.frame.quality)) {
        distance({.kind = PaintKind::GlyphHalo,
                  .mode = PaintMode::Halo,
                  .color = list.Shade(glow->color, true, glow->strength),
                  .shape0 = GlyphCache::DistanceScale,
                  .style0 = std::min(glow->radius / distanceUnit, distanceReach),
                  .emission = emission});
    }
    if (style.outline.width > 0.0f && style.outline.color.a > 0) {
        const float half = std::min(style.outline.width * 0.5f / distanceUnit, distanceReach * 0.5f);
        distance({.kind = PaintKind::GlyphHalo,
                  .mode = PaintMode::Stroke,
                  .color = list.Shade(style.outline.color),
                  .shape0 = GlyphCache::DistanceScale,
                  .style0 = half,
                  .style1 = half,
                  .emission = emission});
    }
    if (style.color.a > 0) coverage({}, {.color = list.Shade(style.color), .emission = emission});
    return block.Extent();
}
