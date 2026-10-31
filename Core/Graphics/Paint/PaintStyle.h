#pragma once
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRect.h"

// How a stroke sits on a shape's edge: centred on it, entirely inside, or entirely outside.
enum class StrokeAlign { Center, Inside, Outside };
// Line ends. Butt: flat at the end point; Round: half circle; Square: flat, extended by half the width.
enum class StrokeCap { Butt, Round, Square };
// Corners of polylines and paths. Miter falls back to Bevel beyond LineStyle::miterLimit.
enum class StrokeJoin { Miter, Bevel, Round };
// Normal: alpha blending; Add: light adds up (glows); Multiply: darkens; Screen: lightens without saturating.
// Normal and Add share draw calls.
enum class Blend { Normal, Add, Multiply, Screen };

struct StrokeStyle {
    float width = 0.0f; // 0: no stroke
    Color4 color{};
    StrokeAlign align = StrokeAlign::Center;
    StrokeJoin join = StrokeJoin::Miter;
};

// Light around a shape: fades from the shape's edge to 'radius' outside it, added on top of what is below.
// Follows RenderQuality::glow (Reduced: weaker, Off: not drawn).
struct GlowStyle {
    float radius = 0.0f; // 0: no glow
    Color4 color{};
    float strength = 1.0f; // Multiplies the colour's alpha; above 1 brightens the core
};

enum class GradientKind { None, Linear, Radial };
// Fill gradient. Linear: 'from' colour at 'start', 'to' colour at 'end' (shape-local coordinates, i.e. relative to
// the shape's centre); Radial: 'from' at the centre, 'to' at 'radius'. Colours beyond the ends are clamped.
struct Gradient {
    GradientKind kind = GradientKind::None;
    Color4 from{};
    Color4 to{};
    Float2 start{};
    Float2 end{};
    float radius = 0.0f;

    [[nodiscard]] static constexpr Gradient Linear(Float2 start, Float2 end, Color4 from, Color4 to) {
        return {GradientKind::Linear, from, to, start, end, 0.0f};
    }
    [[nodiscard]] static constexpr Gradient Radial(float radius, Color4 from, Color4 to) {
        return {GradientKind::Radial, from, to, {}, {}, radius};
    }
    // Top to bottom across a shape of the given height.
    [[nodiscard]] static constexpr Gradient Vertical(float height, Color4 top, Color4 bottom) {
        return Linear({0.0f, -height * 0.5f}, {0.0f, height * 0.5f}, top, bottom);
    }
};

// Look of a closed shape. Every part is optional and they combine: glow (below), fill, stroke (on top).
struct ShapeStyle {
    Color4 fill{0, 0, 0, 0}; // Alpha 0: no fill (unless a gradient is set)
    Gradient gradient{};     // Replaces 'fill' when set
    StrokeStyle stroke{};
    GlowStyle glow{};
    float emission = 0.0f; // 0..1: share of this shape that feeds bloom
};
[[nodiscard]] constexpr ShapeStyle Fill(Color4 color, float emission = 0.0f) {
    return {.fill = color, .emission = emission};
}
[[nodiscard]] constexpr ShapeStyle Outline(float width, Color4 color, StrokeAlign align = StrokeAlign::Center) {
    return {.stroke = {width, color, align}};
}

// Look of an open line (segment, polyline, path stroke).
struct LineStyle {
    float width = 1.0f;
    Color4 color{255, 255, 255, 255};
    StrokeCap cap = StrokeCap::Butt;
    StrokeJoin join = StrokeJoin::Miter;
    float miterLimit = 4.0f; // Miter length / half width beyond which a Miter join is drawn as Bevel
    GlowStyle glow{};        // Segments only
    float emission = 0.0f;
};

// Drawing a texture into 'dest'. 'source' is in texture pixels (empty: the whole texture); rotation is around the
// destination's centre.
struct ImageStyle {
    FloatRect source{};
    float rotation = 0.0f; // Radians, clockwise
    Color4 tint{255, 255, 255, 255};
    float emission = 0.0f;
};

// Nine-slice borders in texture pixels; the destination keeps them at 'scale' times their size.
struct NineSliceStyle {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float scale = 1.0f;
    Color4 tint{255, 255, 255, 255};
};

// A soft radial light (Painter::Glow).
struct LightStyle {
    Color4 color{255, 255, 255, 255};
    float strength = 1.0f;
    float emission = 0.0f;
};
