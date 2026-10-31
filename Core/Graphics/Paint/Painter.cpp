#include "Graphics/Paint/Painter.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include "Graphics/Paint/Internals/PaintContext.h"
#include "Graphics/Paint/Internals/PaintShared.h"
#include "Graphics/Paint/Path.hpp"
#include "Graphics/Render/Internals/FrameExecutor.h"
#include "Graphics/Render/Internals/RenderResources.h"
#include "Graphics/Surface/Internals/SurfaceState.h"
#include "Graphics/Surface/Internals/TextureRefAccess.h"
#include "Graphics/Surface/Surface.hpp"
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
    Float2 half; // Extent of the shape around its centre
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
    const std::array<Float2, 4> local{Float2{-x, -y}, Float2{x, -y}, Float2{x, y}, Float2{-x, y}};
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
    RequireWidth(style.stroke.width, "Stroke width must not be negative");
    DrawList& list = context.list;
    const std::uint8_t emission = EmissionByte(style.emission);
    if (const auto glow = EffectiveGlow(style.glow, context.frame.quality)) {
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
    RequireWidth(style.width, "Line width must not be negative");
    if (style.width <= 0.0f || style.color.a == 0) return;
    const TargetScale scale = Measure(context.list);
    const StrokeParams params{style.width * scale.scale, style.cap, style.join, std::max(style.miterLimit, 1.0f),
                              scale.aa};
    StrokePolyline(context.mesh, points, closed, params, context.list.Shade(style.color));
    SubmitMesh(context, EmissionByte(style.emission));
}

void ShadeTargetPolygon(PaintContext& context, std::span<const Float2> points, const ShapeStyle& style) {
    DrawList& list = context.list;
    const TargetScale scale = Measure(list);
    const std::uint8_t emission = EmissionByte(style.emission);
    if (const auto glow = EffectiveGlow(style.glow, context.frame.quality)) {
        PolygonHalo(context.mesh, points, glow->radius * scale.scale, list.Shade(glow->color, true, glow->strength));
        SubmitMesh(context, emission);
    }
    const Color4 fill = style.gradient.kind != GradientKind::None ? style.gradient.from : style.fill;
    if (fill.a > 0) {
        FillPolygon(context.mesh, points, list.Shade(fill), scale.aa);
        SubmitMesh(context, emission);
    }
    RequireWidth(style.stroke.width, "Stroke width must not be negative");
    if (style.stroke.width > 0.0f && style.stroke.color.a > 0) {
        StrokeTargetPoints(context, points, true,
                           {.width = style.stroke.width,
                            .color = style.stroke.color,
                            .join = StrokeJoin::Miter,
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
    if (context.legacyDirty && context.legacy) {
        context.legacy->Flush();
        context.resources.Device().InvalidateState();
        context.legacyDirty = false;
    }
    if (context.list.HasPending()) {
        context.executor.Execute(context.list);
        context.legacyBound.reset();
    }
}

Painter::Painter(PaintContext& context)
    : _context(context) {}

PaintScope::~PaintScope() {
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
    RequireWidth(style.width, "Line width must not be negative");
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
    shape.half = {radius + half, radius + half};
    shape.shape0 = radius;
    shape.shape1 = half;
    shape.shape2 = std::abs(sweep) * 0.5f;
    shape.flags = style.cap == StrokeCap::Round ? PaintCapRound : std::uint8_t{0};
    DrawShape(_context, shape, {.fill = style.color, .glow = style.glow, .emission = style.emission});
}

void Painter::Line(Float2 from, Float2 to, const LineStyle& style) {
    RequireWidth(style.width, "Line width must not be negative");
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
    ShadeTargetPolygon(_context, _context.points, style);
}

void Painter::Triangle(Float2 a, Float2 b, Float2 c, const ShapeStyle& style) {
    const std::array points{a, b, c};
    Polygon(points, style);
}

void Painter::FillPath(const Path& path, const ShapeStyle& style) {
    if (path.Empty()) return;
    DrawList& list = _context.list;
    const Float2 pixels = list.CurrentTarget().PixelScale();
    const float tolerance = FlattenPixels / std::max(std::min(pixels.x, pixels.y), 1e-6f);
    FlattenPath(path, list.State().transform, tolerance, _context.points, _context.contours);
    for (const Contour& contour : _context.contours) {
        if (contour.count < 3) continue;
        ShadeTargetPolygon(_context, std::span<const Float2>(_context.points).subspan(contour.first, contour.count),
                           style);
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
    case GlowQuality::Full   : break;
    case GlowQuality::Reduced: strength *= ReducedGlowStrength; break;
    case GlowQuality::Off    : return;
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
    if (transform.b != 0.0f || transform.c != 0.0f)
        throw std::invalid_argument("Painter::Clip: the current transform rotates");
    const FloatRect mapped =
        FloatRect::FromCorners(transform.Apply(rect.Position()), transform.Apply({rect.Right(), rect.Bottom()}));
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

PaintScope Painter::Legacy() {
    PaintContext& context = _context;
    if (!context.legacy) throw std::logic_error("Painter::Legacy: no raylib bridge in this renderer");
    DrawList& list = context.list;
    const PaintTarget& target = list.CurrentTarget();
    LegacyBinding binding;
    binding.target = target.target.Valid() ? context.resources.Device().Native(target.target)
                                           : GpuNativeTarget{0, 0, target.viewport.width, target.viewport.height};
    binding.size = target.size;
    binding.viewport = target.viewport;
    binding.transform = list.State().transform;
    if (const auto& clip = list.State().clip) {
        const Float2 scale = target.PixelScale();
        const int left = static_cast<int>(std::floor(clip->x * scale.x));
        const int top = static_cast<int>(std::floor(clip->y * scale.y));
        binding.scissor = PixelRect{target.viewport.x + left, target.viewport.y + top,
                                    static_cast<int>(std::ceil(clip->Right() * scale.x)) - left,
                                    static_cast<int>(std::ceil(clip->Bottom() * scale.y)) - top};
    }
    if (list.HasPending()) FlushPaint(context);
    if (!context.legacyBound || *context.legacyBound != binding) {
        context.legacy->Bind(binding);
        context.legacyBound = binding;
        ++context.legacySwitches;
    }
    context.legacyDirty = true;
    const std::uint32_t depth = list.PushScope(ScopeKind::Legacy);
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
