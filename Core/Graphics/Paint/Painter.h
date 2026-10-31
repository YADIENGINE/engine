#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include "Graphics/Paint/PaintStyle.h"
#include "Graphics/Post/PostSettings.h"
#include "Graphics/Post/RenderQuality.h"
#include "Graphics/Surface/TextureRef.h"
#include "Utilities/Affine2.h"
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRect.h"

class FontFace;
class Painter;
class Path;
class Surface;
class TextBlock;
struct TextStyle;
struct PaintContext;

// A temporary Painter state (transform, clip, opacity, blend, group, target, marker). It lasts until
// the object is destroyed, then the previous state returns. Keep it in a local variable:
//     auto clip = paint.Clip(area);
// It can be moved (the moved-from scope ends nothing), so a scope can be conditional or returned by a helper:
//     std::optional<PaintScope> into;
//     if (layered) into.emplace(paint.Target(layer));
// Scopes end in the reverse order they began; ending one early (destroying it out of order) is a programming error
// reported when the frame ends.
class [[nodiscard]] PaintScope {
public:
    PaintScope(PaintScope&& other) noexcept
        : _painter(std::exchange(other._painter, nullptr))
        , _depth(other._depth) {}
    ~PaintScope();

private:
    friend class Painter;
    PaintScope(Painter& painter, std::uint32_t depth)
        : _painter(&painter)
        , _depth(depth) {}
    Painter* _painter; // Null once moved from
    std::uint32_t _depth;

public:
    PaintScope(const PaintScope&) = delete;
    PaintScope& operator=(const PaintScope&) = delete;
    // Assigning would end the replaced scope at an arbitrary point: use std::optional::emplace.
    PaintScope& operator=(PaintScope&&) = delete;
};

// Draws the group into a temporary image first, then composites it in one piece: overlapping parts do not add up
// (a fading sprite made of several shapes fades as a whole).
struct GroupStyle {
    float opacity = 1.0f;
    Blend blend = Blend::Normal;
    float emission = 0.0f; // Extra bloom share of the whole group
};

// Records drawing for one frame (or one offscreen surface). Coordinates are logical canvas units (y down), angles
// radians clockwise. Every shape is anti-aliased and positioned with sub-pixel precision; nothing is drawn until the
// frame ends, but the order of calls is always kept. Degenerate input (zero size, NaN position) draws nothing.
// Programmer errors (negative widths, mismatched spans, a clip under rotation, scopes ended out of order) throw in
// Debug builds (invalid_argument at the call, logic_error for scopes at the frame's end); Release builds skip the
// drawing and count it (RenderStats::paintErrors).
class Painter {
public:
    explicit Painter(PaintContext& context);

    // --- Shapes ------------------------------------------------------------------------------------------------
    void Rect(const FloatRect& rect, const ShapeStyle& style);
    void RoundRect(const FloatRect& rect, float radius, const ShapeStyle& style);
    // Rectangle with its corners cut off at 45 degrees ('chamfer' along each edge).
    void ChamferRect(const FloatRect& rect, float chamfer, const ShapeStyle& style);
    void Circle(Float2 center, float radius, const ShapeStyle& style);
    void Ellipse(Float2 center, Float2 radii, const ShapeStyle& style);
    // Part of a circle's outline from 'startAngle' over 'sweep' (clockwise when positive), 'style.width' thick.
    // Caps: Butt or Round. A full turn draws a ring.
    void Arc(Float2 center, float radius, float startAngle, float sweep, const LineStyle& style);
    void Line(Float2 from, Float2 to, const LineStyle& style);
    void Polyline(std::span<const Float2> points, const LineStyle& style, bool closed = false);
    // Simple (non-self-intersecting) polygon: fill (no gradient), stroke and glow are supported.
    void Polygon(std::span<const Float2> points, const ShapeStyle& style);
    void Triangle(Float2 a, Float2 b, Float2 c, const ShapeStyle& style);
    // Triangles ('indices' in threes) with one colour per point, blended across each triangle: soft fields such as
    // light masks. Not anti-aliased, so neighbouring triangles meet without seams. Index out of range, a count not a
    // multiple of three or 'colors' not matching 'points' is invalid_argument.
    void Mesh(std::span<const Float2> points, std::span<const Color4> colors, std::span<const std::uint32_t> indices,
              float emission = 0.0f);
    void FillPath(const Path& path, const ShapeStyle& style);
    void StrokePath(const Path& path, const LineStyle& style);
    // Soft round light fading from 'center' to 'radius', added on top (follows RenderQuality::glow).
    void Glow(Float2 center, float radius, const LightStyle& light);

    // --- Images ------------------------------------------------------------------------------------------------
    void Image(const TextureRef& texture, const FloatRect& dest, const ImageStyle& style = {});
    // Whole surface into 'dest'; the surface must have been drawn into this frame or earlier.
    void Image(const Surface& surface, const FloatRect& dest, const ImageStyle& style = {});
    // Convex polygon showing part of 'surface': 'sources[i]' is where 'points[i]' samples the surface, in its logical
    // units (as ImageStyle::source), so a piece of an image can be warped onto any convex shape (shards, ripples).
    // Not anti-aliased. Point and source counts must match (invalid_argument); fewer than three draws nothing.
    void ImagePolygon(const Surface& surface, std::span<const Float2> points, std::span<const Float2> sources,
                      Color4 tint = {255, 255, 255, 255}, float emission = 0.0f);
    void NineSlice(const TextureRef& texture, const FloatRect& dest, const NineSliceStyle& style);

    // --- Text --------------------------------------------------------------------------------------------------
    // 'position' is the top of the first line; horizontally the line's left end, centre or right end (style.align).
    // Repeated strings are laid out once (cached); glyphs are rasterized for the size they cover on screen.
    // Without a font argument the game's font is used (Font()). Each returns the laid-out block's size (after
    // wrapping and fitting), so stacked paragraphs can advance by it.
    Float2 Text(std::string_view text, Float2 position, const TextStyle& style);
    Float2 Text(const FontFace& font, std::string_view text, Float2 position, const TextStyle& style);
    // A block from font.Layout (its size, wrapping and fitting win over 'style'; colour and effects come from it).
    Float2 Text(const FontFace& font, const TextBlock& block, Float2 position, const TextStyle& style);
    // Text wrapped or fitted to 'area' (style.maxWidth = area width), aligned inside it, starting at its top.
    Float2 TextBox(std::string_view text, const FloatRect& area, const TextStyle& style);
    Float2 TextBox(const FontFace& font, std::string_view text, const FloatRect& area, const TextStyle& style);
    // The game's font (GameSettings::font): widgets and text without a font argument use it. logic_error when the
    // game has none.
    [[nodiscard]] const FontFace& Font() const;
    [[nodiscard]] bool HasFont() const;

    // --- State scopes ------------------------------------------------------------------------------------------
    // Following drawing goes through 'transform' (applied before the current one).
    PaintScope Transform(const Affine2& transform);
    // Following drawing is clipped to 'rect' (current coordinates) intersected with the current clip. The transform
    // must not rotate (90-degree multiples included); otherwise invalid_argument.
    PaintScope Clip(const FloatRect& rect);
    // Multiplies the alpha of following drawing. Overlapping parts show through each other; see Group.
    PaintScope Opacity(float opacity);
    PaintScope Blending(Blend blend);
    PaintScope Group(const GroupStyle& style = {});
    // Following drawing goes into 'surface' (fresh state: no transform, clip or opacity); cleared first unless
    // 'clear' is empty.
    PaintScope Target(Surface& surface, std::optional<Color4> clear = Color4{0, 0, 0, 0});
    // Names the following GPU work in captures and the profiler. 'name' must outlive the frame (a literal).
    PaintScope Marker(std::string_view name);

    // --- Frame information -------------------------------------------------------------------------------------
    // Presentation clock in seconds (frozen while the editor pauses) and the frame's step.
    [[nodiscard]] double Time() const;
    [[nodiscard]] float Delta() const;
    [[nodiscard]] const RenderQuality& Quality() const;
    // Device pixels one unit covers under the current transform (for level-of-detail decisions).
    [[nodiscard]] float PixelScale() const;
    // Logical rectangle of the current target (the canvas or a surface) in target coordinates.
    [[nodiscard]] FloatRect Bounds() const;
    // True when 'surface' has no up-to-date image for the current resolution and must be drawn again.
    [[nodiscard]] bool Stale(const Surface& surface) const;
    // This frame's post-processing (starts as the game's defaults every frame).
    [[nodiscard]] PostSettings& Post();

    // --- Errors ------------------------------------------------------------------------------------------------
    // Drawing helpers report invalid arguments the way Painter does: Debug builds throw invalid_argument here;
    // Release builds count it and return, and the helper draws nothing.
    void Reject(std::string_view message);

private:
    friend class PaintScope;
    void EndScope(std::uint32_t depth);
    PaintContext& _context;

public:
    Painter(const Painter&) = delete;
    Painter& operator=(const Painter&) = delete;
    Painter(Painter&&) = delete;
    Painter& operator=(Painter&&) = delete;
};
