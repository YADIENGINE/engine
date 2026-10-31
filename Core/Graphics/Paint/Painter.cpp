#include "Graphics/Paint/Painter.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include "Graphics/Paint/Internals/PaintContext.h"
#include "Graphics/Paint/Internals/PaintShared.h"
#include "Graphics/Paint/Path.h"
#include "Graphics/Render/Internals/FrameExecutor.h"
#include "Graphics/Render/Internals/RenderResources.h"
#include "Graphics/Surface/Internals/SurfaceState.h"
#include "Graphics/Surface/Internals/TextureRefAccess.h"
#include "Graphics/Surface/Surface.h"
#include "Utilities/MathUtils.h"

namespace {
constexpr float AntiAliasPixels = 1.5f; // Quad margin beyond a shape's coverage, device pixels
constexpr float FlattenPixels = 0.25f;  // Curve flattening tolerance, device pixels
constexpr float FullTurn = 2.0f * Pi;
constexpr Color4 White{255, 255, 255, 255};
constexpr PremultipliedColor Transparent{};

bool Drawable(const FloatRect& rect) {
    return Finite(rect.x) && Finite(rect.y) && Finite(rect.width) && Finite(rect.height) && !rect.Empty();
}

std::uint8_t ModeFlags(PaintMode mode) { return static_cast<std::uint8_t>(mode); }

std::uint8_t CapFlags(StrokeCap cap) {
    switch (cap) {
    case StrokeCap::Butt  : return 0;
    case StrokeCap::Round : return PaintCapRound;
    case StrokeCap::Square: return PaintCapSquare;
    }
    return 0;
}

PremultipliedColor Premultiply(Color4 color) {
    const float alpha = static_cast<float>(color.a) / 255.0f;
    return {static_cast<float>(color.r) / 255.0f * alpha, static_cast<float>(color.g) / 255.0f * alpha,
            static_cast<float>(color.b) / 255.0f * alpha, alpha};
}

// A distance shape in its own frame: 'frame' maps shape-local coordinates (centred on the shape) to the current
// drawing coordinates without scaling.
struct ShapeGeometry {
    PaintKind kind = PaintKind::Circle;
    Affine2 frame;
    Float2 half;   // Extent of the drawn quad around 'offset'
    Float2 offset; // Centre of the drawn quad in shape coordinates (the distance function's origin stays at 0)
    float shape0 = 0.0f;
    float shape1 = 0.0f;
    float shape2 = 0.0f;
    std::uint8_t flags = 0;
};

struct ShapeLayer {
    PaintMode mode = PaintMode::Fill;
    float outside = 0.0f; // How far the layer reaches beyond the shape
    PaintColor color;
    PaintColor color2;
    float style0 = 0.0f;
    float style1 = 0.0f;
    std::uint8_t flags = 0;
    std::uint8_t emission = 0;
    const Gradient* linear = nullptr; // Per-vertex gradient position
};

void EmitShape(PaintContext& context, const ShapeGeometry& shape, const ShapeLayer& layer) {
    DrawList& list = context.list;
    const Affine2 toTarget = list.State().transform * shape.frame;
    const float margin = layer.outside + AntiAliasPixels / list.PixelScale();
    const float x = shape.half.x + margin;
    const float y = shape.half.y + margin;
    const Float2 o = shape.offset;
    const std::array<Float2, 4> local{o + Float2{-x, -y}, o + Float2{x, -y}, o + Float2{x, y}, o + Float2{-x, y}};
    std::array<PaintVertex, 4> corners{};
    for (std::size_t i = 0; i < corners.size(); ++i) {
        PaintVertex& vertex = corners[i];
        const Float2 position = toTarget.Apply(local[i]);
        vertex.x = position.x;
        vertex.y = position.y;
        vertex.u = local[i].x;
        vertex.v = local[i].y;
        vertex.shape0 = shape.shape0;
        vertex.shape1 = shape.shape1;
        vertex.shape2 = shape.shape2;
        vertex.style0 = layer.style0;
        vertex.style1 = layer.style1;
        if (layer.linear) {
            const Float2 axis = layer.linear->end - layer.linear->start;
            const float span = std::max(LengthSquared(axis), 1e-12f);
            vertex.style0 = Dot(local[i] - layer.linear->start, axis) / span;
        }
        vertex.color = layer.color;
        vertex.color2 = layer.color2;
        vertex.kind = shape.kind;
        vertex.emission = layer.emission;
        vertex.flags = static_cast<std::uint8_t>(ModeFlags(layer.mode) | shape.flags | layer.flags);
    }
    list.AddQuad(corners);
}

// Glow below, fill, stroke on top.
void DrawShape(PaintContext& context, const ShapeGeometry& shape, const ShapeStyle& style) {
    DrawList& list = context.list;
    if (!ValidWidth(list, style.stroke.width, "Stroke width must not be negative")) return;
    const std::uint8_t emission = EmissionByte(style.emission);
    if (const auto glow = EffectiveGlow(list, style.glow, context.frame.quality)) {
        ShapeLayer layer;
        layer.mode = PaintMode::Halo;
        layer.outside = glow->radius;
        layer.color = list.Shade(glow->color, true, glow->strength);
        layer.style0 = glow->radius;
        layer.emission = emission;
        EmitShape(context, shape, layer);
    }
    const Gradient& gradient = style.gradient;
    if (gradient.kind != GradientKind::None) {
        ShapeLayer layer;
        layer.color = list.Shade(gradient.from);
        layer.color2 = list.Shade(gradient.to);
        layer.emission = emission;
        if (gradient.kind == GradientKind::Linear) {
            layer.flags = PaintGradientLinear;
            layer.linear = &gradient;
        } else {
            layer.flags = PaintGradientRadial;
            layer.style0 = gradient.radius > 0.0f ? 1.0f / gradient.radius : 0.0f;
        }
        EmitShape(context, shape, layer);
    } else if (style.fill.a > 0) {
        EmitShape(context, shape, {.color = list.Shade(style.fill), .emission = emission});
    }
    if (style.stroke.width > 0.0f && style.stroke.color.a > 0) {
        const float half = style.stroke.width * 0.5f;
        const float offset = style.stroke.align == StrokeAlign::Inside    ? -half
                             : style.stroke.align == StrokeAlign::Outside ? half
                                                                          : 0.0f;
        EmitShape(context, shape,
                  {.mode = PaintMode::Stroke,
                   .outside = std::max(0.0f, offset + half),
                   .color = list.Shade(style.stroke.color),
                   .style0 = half,
                   .style1 = offset,
                   .emission = emission});
    }
}

void DrawBox(PaintContext& context, PaintKind kind, const FloatRect& rect, float corner, const ShapeStyle& style) {
    if (!Drawable(rect) || !Finite(corner)) return;
    const Float2 half = rect.Size() * 0.5f;
    const float limit = std::min(half.x, half.y);
    ShapeGeometry shape;
    shape.kind = kind;
    shape.frame = Affine2::Translate(rect.Center());
    shape.half = half;
    shape.shape0 = half.x;
    shape.shape1 = half.y;
    shape.shape2 = std::clamp(corner, 0.0f, limit);
    DrawShape(context, shape, style);
}

// Line width and anti-aliasing fringe in target units for tessellated geometry.
struct TargetScale {
    float aa = 1.0f;
    float scale = 1.0f; // Target units per drawing unit
};
TargetScale Measure(const DrawList& list) {
    const Float2 pixels = list.CurrentTarget().PixelScale();
    return {1.0f / std::max(std::min(pixels.x, pixels.y), 1e-6f), list.State().transform.UniformScale()};
}

void TransformPoints(PaintContext& context, std::span<const Float2> points) {
    const Affine2& transform = context.list.State().transform;
    context.points.clear();
    for (const Float2 point : points)
        context.points.push_back(transform.Apply(point));
}

void SubmitMesh(PaintContext& context, std::uint8_t emission) {
    MeshBuffer& mesh = context.mesh;
    for (PaintVertex& vertex : mesh.vertices)
        vertex.emission = emission;
    context.list.Add(mesh.vertices, mesh.indices);
    mesh.Clear();
}

void StrokeTargetPoints(PaintContext& context, std::span<const Float2> points, bool closed, const LineStyle& style) {
    if (!ValidWidth(context.list, style.width, "Line width must not be negative")) return;
    if (style.width <= 0.0f || style.color.a == 0) return;
    const TargetScale scale = Measure(context.list);
    const StrokeParams params{style.width * scale.scale, style.cap, style.join, std::max(style.miterLimit, 1.0f),
                              scale.aa};
    StrokePolyline(context.mesh, points, closed, params, context.list.Shade(style.color));
    SubmitMesh(context, EmissionByte(style.emission));
}

Float2 BoundsCenter(std::span<const Float2> points, const Affine2& transform = {}) {
    if (points.empty()) return {};
    Float2 low = transform.Apply(points.front());
    Float2 high = low;
    for (const Float2 point : points) {
        const Float2 at = transform.Apply(point);
        low = {std::min(low.x, at.x), std::min(low.y, at.y)};
        high = {std::max(high.x, at.x), std::max(high.y, at.y)};
    }
    return (low + high) * 0.5f;
}

void ShadeTargetPolygon(PaintContext& context, std::span<const Float2> points, const ShapeStyle& style,
                        Float2 gradientCenter) {
    DrawList& list = context.list;
    if (!ValidWidth(list, style.stroke.width, "Stroke width must not be negative")) return;
    const TargetScale scale = Measure(list);
    const std::uint8_t emission = EmissionByte(style.emission);
    if (const auto glow = EffectiveGlow(list, style.glow, context.frame.quality)) {
        PolygonHalo(context.mesh, points, glow->radius * scale.scale, list.Shade(glow->color, true, glow->strength));
        SubmitMesh(context, emission);
    }
    if (style.gradient.kind != GradientKind::None) {
        const Affine2 toLocal = Affine2::Translate(-gradientCenter) * list.State().transform.Inverse();
        FillPolygon(context.mesh, points, list.Shade(style.gradient.from), list.Shade(style.gradient.to),
                    style.gradient, toLocal, scale.aa);
        SubmitMesh(context, emission);
    } else if (style.fill.a > 0) {
        FillPolygon(context.mesh, points, list.Shade(style.fill), scale.aa);
        SubmitMesh(context, emission);
    }
    if (style.stroke.width > 0.0f && style.stroke.color.a > 0) {
        StrokeTargetPoints(context, points, true,
                           {.width = style.stroke.width,
                            .color = style.stroke.color,
                            .join = style.stroke.join,
                            .emission = style.emission});
    }
}

void AddImageQuad(PaintContext& context, const Affine2& frame, Float2 half, const FloatRect& uv, bool flipY,
                  PaintColor color, std::uint8_t emission, std::span<const GpuTexture> textures) {
    DrawList& list = context.list;
    const Affine2 toTarget = list.State().transform * frame;
    const std::array<Float2, 4> local{Float2{-half.x, -half.y}, Float2{half.x, -half.y}, Float2{half.x, half.y},
                                      Float2{-half.x, half.y}};
    const std::array<Float2, 4> coords{Float2{uv.x, uv.y}, Float2{uv.Right(), uv.y}, Float2{uv.Right(), uv.Bottom()},
                                       Float2{uv.x, uv.Bottom()}};
    std::array<PaintVertex, 4> corners{};
    for (std::size_t i = 0; i < corners.size(); ++i) {
        PaintVertex& vertex = corners[i];
        const Float2 position = toTarget.Apply(local[i]);
        vertex.x = position.x;
        vertex.y = position.y;
        vertex.u = coords[i].x;
        vertex.v = flipY ? 1.0f - coords[i].y : coords[i].y;
        vertex.color = color;
        vertex.kind = textures.size() > 1 ? PaintKind::Composite : PaintKind::Texture;
        vertex.slot = 0;      // Colour: textures[0]
        vertex.shape0 = 1.0f; // Composite emission: textures[1] (DrawList maps both to batch slots)
        vertex.emission = emission;
    }
    list.AddQuad(corners, textures);
}
} // namespace

void FlushPaint(PaintContext& context) {
    if (context.list.HasPending()) context.executor.Execute(context.list);
}

Painter::Painter(PaintContext& context)
    : _context(context) {}

PaintScope::~PaintScope() {
    if (!_painter) return;
    try {
        _painter->EndScope(_depth);
    } catch (const std::exception& error) {
        _painter->_context.list.Fail(error.what()); // Reported when the frame finishes
    }
}

// --- Shapes ---

void Painter::Rect(const FloatRect& rect, const ShapeStyle& style) {
    DrawBox(_context, PaintKind::RoundRect, rect, 0.0f, style);
}

void Painter::RoundRect(const FloatRect& rect, float radius, const ShapeStyle& style) {
    DrawBox(_context, PaintKind::RoundRect, rect, radius, style);
}

void Painter::ChamferRect(const FloatRect& rect, float chamfer, const ShapeStyle& style) {
    DrawBox(_context, PaintKind::ChamferRect, rect, chamfer, style);
}

void Painter::Circle(Float2 center, float radius, const ShapeStyle& style) {
    if (!Finite(center) || !(radius > 0.0f) || !Finite(radius)) return;
    ShapeGeometry shape;
    shape.kind = PaintKind::Circle;
    shape.frame = Affine2::Translate(center);
    shape.half = {radius, radius};
    shape.shape0 = radius;
    DrawShape(_context, shape, style);
}

void Painter::Ellipse(Float2 center, Float2 radii, const ShapeStyle& style) {
    if (!Finite(center) || !Finite(radii) || !(radii.x > 0.0f) || !(radii.y > 0.0f)) return;
    if (radii.x == radii.y) {
        Circle(center, radii.x, style);
        return;
    }
    ShapeGeometry shape;
    shape.kind = PaintKind::Ellipse;
    shape.frame = Affine2::Translate(center);
    shape.half = radii;
    shape.shape0 = radii.x;
    shape.shape1 = radii.y;
    DrawShape(_context, shape, style);
}

void Painter::Arc(Float2 center, float radius, float startAngle, float sweep, const LineStyle& style) {
    if (!ValidWidth(_context.list, style.width, "Line width must not be negative")) return;
    if (!Finite(center) || !(radius > 0.0f) || !Finite(startAngle) || !Finite(sweep) || sweep == 0.0f) return;
    if (style.width <= 0.0f) return;
    if (std::abs(sweep) >= FullTurn) {
        Circle(center, radius,
               {.stroke = {style.width, style.color, StrokeAlign::Center},
                .glow = style.glow,
                .emission = style.emission});
        return;
    }
    const float half = style.width * 0.5f;
    ShapeGeometry shape;
    shape.kind = PaintKind::Arc;
    shape.frame = Affine2::Translate(center) * Affine2::Rotate(startAngle + sweep * 0.5f);
    // The quad covers the arc's sector only (local x points at the arc's middle, it spans +-spread around it).
    const float spread = std::abs(sweep) * 0.5f;
    const float outer = radius + half;
    const float cap = style.cap == StrokeCap::Round ? half : 0.0f;
    const float left =
        std::max(-outer, (spread <= Pi * 0.5f ? std::max(0.0f, radius - half) : outer) * std::cos(spread) - cap);
    const float reach = (spread <= Pi * 0.5f ? outer * std::sin(spread) : outer) + cap;
    shape.half = {(outer - left) * 0.5f, std::min(reach, outer)};
    shape.offset = {(outer + left) * 0.5f, 0.0f};
    shape.shape0 = radius;
    shape.shape1 = half;
    shape.shape2 = std::abs(sweep) * 0.5f;
    shape.flags = style.cap == StrokeCap::Round ? PaintCapRound : std::uint8_t{0};
    DrawShape(_context, shape, {.fill = style.color, .glow = style.glow, .emission = style.emission});
}

void Painter::Line(Float2 from, Float2 to, const LineStyle& style) {
    if (!ValidWidth(_context.list, style.width, "Line width must not be negative")) return;
    if (!Finite(from) || !Finite(to) || style.width <= 0.0f) return;
    const Float2 delta = to - from;
    const float length = Length(delta);
    if (length < 1e-6f && style.cap == StrokeCap::Butt) return;
    const Float2 direction = Normalized(delta, {1.0f, 0.0f});
    const float half = style.width * 0.5f;
    const float extension = style.cap == StrokeCap::Butt ? 0.0f : half;
    ShapeGeometry shape;
    shape.kind = PaintKind::Segment;
    const Float2 middle = Mix(from, to, 0.5f);
    shape.frame = {direction.x, direction.y, -direction.y, direction.x, middle.x, middle.y};
    shape.half = {length * 0.5f + extension, half};
    shape.shape0 = length * 0.5f;
    shape.shape1 = half;
    shape.flags = CapFlags(style.cap);
    DrawShape(_context, shape, {.fill = style.color, .glow = style.glow, .emission = style.emission});
}

void Painter::Polyline(std::span<const Float2> points, const LineStyle& style, bool closed) {
    if (points.size() < 2) return;
    TransformPoints(_context, points);
    StrokeTargetPoints(_context, _context.points, closed, style);
}

void Painter::Polygon(std::span<const Float2> points, const ShapeStyle& style) {
    if (points.size() < 3) return;
    TransformPoints(_context, points);
    ShadeTargetPolygon(_context, _context.points, style, BoundsCenter(points));
}

void Painter::Triangle(Float2 a, Float2 b, Float2 c, const ShapeStyle& style) {
    const std::array points{a, b, c};
    Polygon(points, style);
}

void Painter::Mesh(std::span<const Float2> points, std::span<const Color4> colors,
                   std::span<const std::uint32_t> indices, float emission) {
    DrawList& list = _context.list;
    if (colors.size() != points.size()) return list.Reject("Mesh needs one colour per point");
    if (indices.size() % 3 != 0) return list.Reject("Mesh indices must come in threes");
    for (const std::uint32_t index : indices)
        if (index >= points.size()) return list.Reject("Mesh index out of range");
    if (indices.empty()) return;
    const Affine2& transform = list.State().transform;
    MeshBuffer& mesh = _context.mesh;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Float2 at = transform.Apply(points[i]);
        mesh.vertices.push_back({.x = at.x, .y = at.y, .color = list.Shade(colors[i]), .kind = PaintKind::Solid});
    }
    mesh.indices.assign(indices.begin(), indices.end());
    SubmitMesh(_context, EmissionByte(emission));
}

void Painter::FillPath(const Path& path, const ShapeStyle& style) {
    if (path.Empty()) return;
    DrawList& list = _context.list;
    const Float2 pixels = list.CurrentTarget().PixelScale();
    const float tolerance = FlattenPixels / std::max(std::min(pixels.x, pixels.y), 1e-6f);
    FlattenPath(path, list.State().transform, tolerance, _context.points, _context.contours);
    const Float2 center = BoundsCenter(_context.points, list.State().transform.Inverse());
    for (const Contour& contour : _context.contours) {
        if (contour.count < 3) continue;
        ShadeTargetPolygon(_context, std::span<const Float2>(_context.points).subspan(contour.first, contour.count),
                           style, center);
    }
}

void Painter::StrokePath(const Path& path, const LineStyle& style) {
    if (path.Empty()) return;
    DrawList& list = _context.list;
    const Float2 pixels = list.CurrentTarget().PixelScale();
    const float tolerance = FlattenPixels / std::max(std::min(pixels.x, pixels.y), 1e-6f);
    FlattenPath(path, list.State().transform, tolerance, _context.points, _context.contours);
    for (const Contour& contour : _context.contours) {
        StrokeTargetPoints(_context, std::span<const Float2>(_context.points).subspan(contour.first, contour.count),
                           contour.closed, style);
    }
}

void Painter::Glow(Float2 center, float radius, const LightStyle& light) {
    if (!Finite(center) || !(radius > 0.0f) || light.color.a == 0) return;
    float strength = light.strength;
    switch (_context.frame.quality.glow) {
    case GlowQuality::Full: break;
    case GlowQuality::Reduced:
        radius *= ReducedGlowRadius;
        strength *= ReducedGlowStrength;
        break;
    case GlowQuality::Off: return;
    }
    if (!(strength > 0.0f)) return;
    ShapeGeometry shape;
    shape.kind = PaintKind::Glow;
    shape.frame = Affine2::Translate(center);
    shape.half = {radius, radius};
    shape.shape0 = radius;
    ShapeLayer layer;
    layer.color = _context.list.Shade(light.color, true, strength);
    layer.emission = EmissionByte(light.emission);
    EmitShape(_context, shape, layer);
}

// --- Images ---

void Painter::Image(const TextureRef& texture, const FloatRect& dest, const ImageStyle& style) {
    if (!texture.Valid() || !Drawable(dest)) return;
    const auto width = static_cast<float>(texture.Width());
    const auto height = static_cast<float>(texture.Height());
    const FloatRect source = style.source.Empty() ? FloatRect{0.0f, 0.0f, width, height} : style.source;
    const FloatRect uv{source.x / width, source.y / height, source.width / width, source.height / height};
    const Affine2 frame = Affine2::Translate(dest.Center()) * Affine2::Rotate(style.rotation);
    const std::array textures{TextureRefAccess::Texture(texture)};
    AddImageQuad(_context, frame, dest.Size() * 0.5f, uv, TextureRefAccess::FlipY(texture),
                 _context.list.Shade(style.tint), EmissionByte(style.emission), textures);
}

void Painter::Image(const Surface& surface, const FloatRect& dest, const ImageStyle& style) {
    const SurfaceAccess::State& state = SurfaceAccess::Of(surface);
    if (!state.drawn || !Drawable(dest)) return;
    const FloatRect source = style.source.Empty() ? FloatRect{0.0f, 0.0f, state.size.x, state.size.y} : style.source;
    // Surface sources are logical units; the texture holds the surface's whole logical area.
    const FloatRect uv{source.x / state.size.x, source.y / state.size.y, source.width / state.size.x,
                       source.height / state.size.y};
    const Affine2 frame = Affine2::Translate(dest.Center()) * Affine2::Rotate(style.rotation);
    const PaintColor tint = _context.list.Shade(style.tint);
    const std::uint8_t emission = EmissionByte(style.emission);
    if (state.emission.Valid()) {
        const std::array textures{state.color, state.emission};
        AddImageQuad(_context, frame, dest.Size() * 0.5f, uv, true, tint, emission, textures);
    } else {
        const std::array textures{state.color};
        AddImageQuad(_context, frame, dest.Size() * 0.5f, uv, true, tint, emission, textures);
    }
}

void Painter::ImagePolygon(const Surface& surface, std::span<const Float2> points, std::span<const Float2> sources,
                           Color4 tint, float emission) {
    DrawList& list = _context.list;
    if (sources.size() != points.size()) return list.Reject("ImagePolygon needs one source per point");
    const SurfaceAccess::State& state = SurfaceAccess::Of(surface);
    if (!state.drawn || points.size() < 3) return;
    const Affine2& transform = list.State().transform;
    const PaintColor color = list.Shade(tint);
    const bool composite = state.emission.Valid();
    MeshBuffer& mesh = _context.mesh;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Float2 at = transform.Apply(points[i]);
        PaintVertex vertex{.x = at.x,
                           .y = at.y,
                           .u = sources[i].x / state.size.x,
                           .v = 1.0f - sources[i].y / state.size.y, // Surfaces are stored bottom-up
                           .shape0 = 1.0f,                          // Composite emission: textures[1]
                           .color = color,
                           .kind = composite ? PaintKind::Composite : PaintKind::Texture,
                           .emission = EmissionByte(emission)};
        mesh.vertices.push_back(vertex);
    }
    for (std::uint32_t i = 1; i + 1 < points.size(); ++i)
        mesh.indices.insert(mesh.indices.end(), {0U, i, i + 1});
    if (composite) {
        const std::array textures{state.color, state.emission};
        list.Add(mesh.vertices, mesh.indices, textures);
    } else {
        const std::array textures{state.color};
        list.Add(mesh.vertices, mesh.indices, textures);
    }
    mesh.Clear();
}

void Painter::NineSlice(const TextureRef& texture, const FloatRect& dest, const NineSliceStyle& style) {
    if (!texture.Valid() || !Drawable(dest)) return;
    const auto width = static_cast<float>(texture.Width());
    const auto height = static_cast<float>(texture.Height());
    const float scale = std::max(style.scale, 0.0f);
    const std::array<float, 4> sourceX{0.0f, style.left, width - style.right, width};
    const std::array<float, 4> sourceY{0.0f, style.top, height - style.bottom, height};
    const float left = std::min(style.left * scale, dest.width * 0.5f);
    const float right = std::min(style.right * scale, dest.width * 0.5f);
    const float top = std::min(style.top * scale, dest.height * 0.5f);
    const float bottom = std::min(style.bottom * scale, dest.height * 0.5f);
    const std::array<float, 4> destX{dest.x, dest.x + left, dest.Right() - right, dest.Right()};
    const std::array<float, 4> destY{dest.y, dest.y + top, dest.Bottom() - bottom, dest.Bottom()};
    const PaintColor tint = _context.list.Shade(style.tint);
    const std::array textures{TextureRefAccess::Texture(texture)};
    const bool flip = TextureRefAccess::FlipY(texture);
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            const FloatRect part{destX[column], destY[row], destX[column + 1] - destX[column],
                                 destY[row + 1] - destY[row]};
            if (part.Empty() || sourceX[column + 1] <= sourceX[column] || sourceY[row + 1] <= sourceY[row]) continue;
            const FloatRect uv{sourceX[column] / width, sourceY[row] / height,
                               (sourceX[column + 1] - sourceX[column]) / width,
                               (sourceY[row + 1] - sourceY[row]) / height};
            AddImageQuad(_context, Affine2::Translate(part.Center()), part.Size() * 0.5f, uv, flip, tint, 0, textures);
        }
    }
}

// --- Scopes ---

PaintScope Painter::Transform(const Affine2& transform) {
    const std::uint32_t depth = _context.list.PushScope(ScopeKind::Transform);
    PaintState& state = _context.list.MutableState();
    state.transform = state.transform * transform;
    return {*this, depth};
}

PaintScope Painter::Clip(const FloatRect& rect) {
    DrawList& list = _context.list;
    const Affine2& transform = list.State().transform;
    // A rotated clip is an error; without strict checks the rectangle's bounding box clips instead.
    if (transform.b != 0.0f || transform.c != 0.0f) list.Reject("Painter::Clip: the current transform rotates");
    const FloatRect mapped =
        FloatRect::FromCorners(transform.Apply(rect.Position()), transform.Apply({rect.Right(), rect.Bottom()}))
            .Union(FloatRect::FromCorners(transform.Apply({rect.Right(), rect.y}),
                                          transform.Apply({rect.x, rect.Bottom()})));
    const FloatRect bounds{0.0f, 0.0f, list.CurrentTarget().size.x, list.CurrentTarget().size.y};
    const FloatRect current = list.State().clip.value_or(bounds);
    const std::uint32_t depth = list.PushScope(ScopeKind::Clip);
    list.MutableState().clip = current.Intersect(Drawable(rect) ? mapped : FloatRect{});
    return {*this, depth};
}

PaintScope Painter::Opacity(float opacity) {
    const std::uint32_t depth = _context.list.PushScope(ScopeKind::Opacity);
    PaintState& state = _context.list.MutableState();
    state.opacity *= std::isfinite(opacity) ? std::clamp(opacity, 0.0f, 1.0f) : 0.0f;
    return {*this, depth};
}

PaintScope Painter::Blending(Blend blend) {
    const std::uint32_t depth = _context.list.PushScope(ScopeKind::Blend);
    _context.list.MutableState().blend = blend;
    return {*this, depth};
}

PaintScope Painter::Group(const GroupStyle& style) {
    DrawList& list = _context.list;
    const PaintTarget parent = list.CurrentTarget();
    const PaintTarget image = _context.resources.AcquireTransient(parent.size, parent.viewport, parent.emission);
    const std::uint32_t depth = list.PushScope(ScopeKind::Group);
    list.BeginTarget(image, Transparent, false);
    _context.scopes.push_back({depth, nullptr, image.target, parent, style});
    return {*this, depth};
}

PaintScope Painter::Target(Surface& surface, std::optional<Color4> clear) {
    DrawList& list = _context.list;
    const PaintTarget target = _context.resources.Prepare(surface);
    const std::uint32_t depth = list.PushScope(ScopeKind::Target);
    list.BeginTarget(target, clear ? std::optional{Premultiply(*clear)} : std::nullopt, true);
    _context.scopes.push_back({depth, &surface, {}, {}, {}});
    return {*this, depth};
}

PaintScope Painter::Marker(std::string_view name) {
    const std::uint32_t depth = _context.list.PushScope(ScopeKind::Marker);
    _context.list.PushMarker(name);
    return {*this, depth};
}

void Painter::EndScope(std::uint32_t depth) {
    PaintContext& context = _context;
    DrawList& list = context.list;
    if (depth == 0 || depth > list.Depth()) return;
    const ScopeKind kind = list.KindAt(depth);
    std::optional<PaintScopeRecord> record;
    while (!context.scopes.empty() && context.scopes.back().depth >= depth) {
        if (context.scopes.back().depth == depth) record = context.scopes.back();
        context.scopes.pop_back();
    }
    list.PopScope(depth);
    if (!record) return;
    if (kind == ScopeKind::Target && record->surface) {
        context.resources.MarkDrawn(*record->surface);
        return;
    }
    if (kind != ScopeKind::Group) return;
    // Composite the group image over the parent target, untransformed (the image already holds the transform).
    PaintState& state = list.MutableState();
    const Affine2 transform = state.transform;
    const Blend blend = state.blend;
    state.transform = Affine2::Identity();
    state.blend = record->group.blend;
    const Float2 size = record->parent.size;
    const PaintColor faded = list.Shade(WithAlpha(White, record->group.opacity));
    const FloatRect uv{0.0f, 0.0f, 1.0f, 1.0f};
    const Affine2 frame = Affine2::Translate(size * 0.5f);
    const std::uint8_t emission = EmissionByte(record->group.emission);
    const GpuTexture color = context.resources.Attachment(record->transient, 0);
    if (record->parent.emission) {
        const std::array textures{color, context.resources.Attachment(record->transient, 1)};
        AddImageQuad(context, frame, size * 0.5f, uv, true, faded, emission, textures);
    } else {
        const std::array textures{color};
        AddImageQuad(context, frame, size * 0.5f, uv, true, faded, emission, textures);
    }
    state.transform = transform;
    state.blend = blend;
    context.resources.ReleaseTransient(record->transient);
}

// --- Frame information ---

double Painter::Time() const { return _context.frame.time; }

float Painter::Delta() const { return _context.frame.delta; }

const RenderQuality& Painter::Quality() const { return _context.frame.quality; }

float Painter::PixelScale() const { return _context.list.PixelScale(); }

FloatRect Painter::Bounds() const {
    const Float2 size = _context.list.CurrentTarget().size;
    return {0.0f, 0.0f, size.x, size.y};
}

bool Painter::Stale(const Surface& surface) const { return _context.resources.Stale(surface); }

PostSettings& Painter::Post() { return _context.frame.post; }

void Painter::Reject(std::string_view message) { _context.list.Reject(message); }
