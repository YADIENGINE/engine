#include "Graphics/Draw/DrawUtils.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Paint/Path.h"
#include "Utilities/MathUtils.h"

namespace {
constexpr float StarInnerRatio = 0.45f;
constexpr int StarPoints = 5;
} // namespace

void DrawStar(Painter& paint, Float2 center, float outerRadius, const ShapeStyle& style) {
    std::array<Float2, StarPoints * 2> corners{};
    for (std::size_t i = 0; i < corners.size(); ++i) {
        const float radius = i % 2 == 0 ? outerRadius : outerRadius * StarInnerRatio;
        corners[i] = center + Float2::FromAngle(-Pi / 2.0f + static_cast<float>(i) * Pi / StarPoints, radius);
    }
    paint.Polygon(corners, style);
}

void DrawDiamond(Painter& paint, Float2 center, Float2 half, const ShapeStyle& style) {
    const std::array corners{center - Float2{0.0f, half.y}, center + Float2{half.x, 0.0f},
                             center + Float2{0.0f, half.y}, center - Float2{half.x, 0.0f}};
    paint.Polygon(corners, style);
}

void DrawRegularPolygon(Painter& paint, Float2 center, int sides, float radius, float radians,
                        const ShapeStyle& style) {
    if (sides < 3) return;
    Path path;
    path.AddRegularPolygon(center, sides, radius, radians);
    paint.FillPath(path, style);
}

void DrawBand(Painter& paint, Float2 center, float inner, float outer, float start, float sweep,
              const LineStyle& style) {
    if (outer <= inner) return;
    LineStyle band = style;
    band.width = outer - inner;
    paint.Arc(center, (inner + outer) * 0.5f, start, sweep, band);
}

void DrawRing(Painter& paint, Float2 center, float inner, float outer, const ShapeStyle& style) {
    if (outer <= inner) return;
    ShapeStyle ring = style;
    ring.stroke.width = outer - inner;
    paint.Circle(center, (inner + outer) * 0.5f, ring);
}

void DrawRotatedRect(Painter& paint, Float2 center, Float2 half, float radians, const ShapeStyle& style) {
    const std::array corners{Float2{-half.x, -half.y}, Float2{half.x, -half.y}, Float2{half.x, half.y},
                             Float2{-half.x, half.y}};
    std::array<Float2, 4> turned{};
    for (std::size_t i = 0; i < corners.size(); ++i)
        turned[i] = center + Rotated(corners[i], radians);
    paint.Polygon(turned, style);
}

void DrawLightning(Painter& paint, Float2 from, Float2 to, int seed, Color4 color, float alpha,
                   const LightningStyle& style) {
    if (style.segments <= 0) return;
    std::vector<Float2> points{from};
    points.reserve(static_cast<std::size_t>(style.segments) + 1);
    for (int i = 1; i <= style.segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(style.segments);
        const float wobble = i == style.segments ? 0.0f : std::sin(static_cast<float>(i * 17 + seed * 31) * 12.9898f);
        points.push_back(Mix(from, to, t) + Float2{wobble * style.jitter * std::sin(t * Pi), 0.0f});
    }
    const auto line = [&](float width, Color4 lineColor) {
        paint.Polyline(
            points,
            {.width = width, .color = WithAlpha(lineColor, alpha), .cap = StrokeCap::Round, .join = StrokeJoin::Round});
    };
    line(style.width, color);
    if (style.coreWidth > 0.0f) line(style.coreWidth, style.core);
    if (style.glowRadius > 0.0f) paint.Glow(to, style.glowRadius, {.color = WithAlpha(color, style.glowAlpha * alpha)});
}

void DrawTabOutline(Painter& paint, const FloatRect& tab, float thickness, Color4 color) {
    const std::array corners{Float2{tab.x, tab.Bottom()}, tab.Position(), Float2{tab.Right(), tab.y},
                             Float2{tab.Right(), tab.Bottom()}};
    paint.Polyline(corners, {.width = thickness, .color = color});
}

void DrawTabbedPanelOutline(Painter& paint, const FloatRect& panel, const FloatRect& activeTab, float thickness,
                            Color4 color) {
    const std::array corners{Float2{activeTab.x, panel.y},    panel.Position(),
                             Float2{panel.x, panel.Bottom()}, Float2{panel.Right(), panel.Bottom()},
                             Float2{panel.Right(), panel.y},  Float2{activeTab.Right(), panel.y}};
    paint.Polyline(corners, {.width = thickness, .color = color});
}
