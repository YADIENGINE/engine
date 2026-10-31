#pragma once
#include <cstddef>
#include <cstdint>

// What the paint shader does with a vertex's attributes. Values are part of the shader contract (Paint.glsl).
//   Solid       colour as is (tessellated geometry; anti-aliasing is in the vertex alpha)
//   Texture     premultiplied texture sample x colour
//   Glyph       colour x coverage (R channel of a glyph atlas)
//   GlyphHalo   soft halo from a signed-distance glyph atlas
//   Circle .. Arc   signed-distance shapes evaluated per pixel, see PaintMode
//   Glow        radial light that fades from the centre to shape.x
//   Composite   like Texture, and the bloom output samples the texture in slot shape.x (a rendered image's emission)
enum class PaintKind : std::uint8_t {
    Solid,
    Texture,
    Glyph,
    GlyphHalo,
    Circle,
    Ellipse,
    RoundRect,
    ChamferRect,
    Segment,
    Arc,
    Glow,
    Composite
};
// How a distance shape is covered. Fill: inside; Stroke: a band around the edge; Halo: light fading outwards.
enum class PaintMode : std::uint8_t { Fill, Stroke, Halo };

// Bits of PaintVertex::flags.
inline constexpr std::uint8_t PaintModeMask = 0x03;       // PaintMode
inline constexpr std::uint8_t PaintGradientLinear = 0x04; // Fill: colour -> colour2 along style
inline constexpr std::uint8_t PaintGradientRadial = 0x08; // Fill: colour -> colour2 from the shape centre
inline constexpr std::uint8_t PaintCapRound = 0x10;       // Segment / Arc: round ends (else flat)
inline constexpr std::uint8_t PaintCapSquare = 0x20;      // Segment: ends extended by half the width
inline constexpr std::uint8_t PaintCoverage = 0x40;       // Tessellated gradient: AA coverage in shape3

struct PaintColor {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;
    friend constexpr bool operator==(const PaintColor&, const PaintColor&) = default;
};

// One vertex of the paint pipeline (52 bytes). Positions are in the pass's logical coordinates with every transform
// already applied; 'local' is the shape's own coordinate system (distance shapes) or the texture coordinate.
//   shape  Circle: radius | Ellipse: rx, ry | RoundRect / ChamferRect: half width, half height, corner |
//          Segment: half length, half width | Arc: radius, half thickness, half sweep | Glow: radius |
//          GlyphHalo: distance units per texel | Tessellated gradient: shape3 is AA coverage
//   style  Stroke: half width, centre offset | Halo: radius, strength | Fill with gradient: direction / span (linear)
//          or 1 / radius (radial) | GlyphHalo: radius, strength
struct PaintVertex {
    float x = 0.0f;
    float y = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    float shape0 = 0.0f;
    float shape1 = 0.0f;
    float shape2 = 0.0f;
    float shape3 = 0.0f;
    float style0 = 0.0f;
    float style1 = 0.0f;
    PaintColor color;  // Premultiplied
    PaintColor color2; // Premultiplied gradient end
    PaintKind kind = PaintKind::Solid;
    std::uint8_t slot = 0;     // Texture slot of the batch (0..PaintTextureSlots-1)
    std::uint8_t emission = 0; // Share written to the bloom source (0..255)
    std::uint8_t flags = 0;
};
static_assert(sizeof(PaintVertex) == 52);
static_assert(offsetof(PaintVertex, color) == 40);

// Textures a single draw call can sample; a batch needing more is split.
inline constexpr std::size_t PaintTextureSlots = 8;
