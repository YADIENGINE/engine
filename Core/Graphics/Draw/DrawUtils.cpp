#include "Graphics/Draw/DrawUtils.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
#include "Graphics/Paint/Painter.h"
#include "Utilities/MathUtils.h"

namespace {
constexpr float StarInnerRatio = 0.45f;
constexpr int StarPoints = 5;
constexpr int EggSegments = 48;
constexpr float EggWidthRatio = 0.75f;           // Width / height
constexpr Float2 EggShineOffset{-0.35f, -0.35f}; // Times the half width / half height
constexpr Float2 EggShineRadii{0.25f, 0.3f};
} // namespace

void DrawStar(Painter& paint, Float2 center, float outerRadius, const ShapeStyle& style) {
    std::array<Float2, StarPoints * 2> corners{};
    for (std::size_t i = 0; i < corners.size(); ++i) {
        const float radius = i % 2 == 0 ? outerRadius : outerRadius * StarInnerRatio;
        corners[i] = center + Float2::FromAngle(-Pi / 2.0f + static_cast<float>(i) * Pi / StarPoints, radius);
    }
    paint.Polygon(corners, style);
}

// The top is half an ellipse, the bottom half a circle of the egg's half width: one smooth convex outline.
void DrawEgg(Painter& paint, Float2 center, float height, Color4 color, Color4 shine) {
    if (!(height > 0.0f)) return;
    const float halfWidth = height * EggWidthRatio * 0.5f;
    const float top = height - halfWidth; // Vertical radius of the upper half
    const float equator = center.y - height * 0.5f + top;
    std::array<Float2, EggSegments> outline{};
    for (std::size_t i = 0; i < outline.size(); ++i) {
        const float angle = 2.0f * Pi * static_cast<float>(i) / static_cast<float>(outline.size());
        const float s = std::sin(angle);
        outline[i] = {center.x + halfWidth * std::cos(angle), equator + (s < 0.0f ? top : halfWidth) * s};
    }
    paint.Polygon(outline, Fill(color));
    const Float2 halfSize{halfWidth, height * 0.5f};
    paint.Ellipse(center + Float2{EggShineOffset.x * halfSize.x, EggShineOffset.y * halfSize.y},
                  {EggShineRadii.x * halfSize.x, EggShineRadii.y * halfSize.y}, Fill(shine));
}

void DrawDiamond(Painter& paint, Float2 center, Float2 half, const ShapeStyle& style) {
    const std::array corners{center - Float2{0.0f, half.y}, center + Float2{half.x, 0.0f},
                             center + Float2{0.0f, half.y}, center - Float2{half.x, 0.0f}};
    paint.Polygon(corners, style);
}

void DrawRegularPolygon(Painter& paint, Float2 center, int sides, float radius, float radians,
                        const ShapeStyle& style) {
    if (sides < 3) return;
    std::vector<Float2> corners(static_cast<std::size_t>(sides));
    for (std::size_t i = 0; i < corners.size(); ++i)
        corners[i] =
            center + Float2::FromAngle(radians + static_cast<float>(i) * 2.0f * Pi / static_cast<float>(sides), radius);
    paint.Polygon(corners, style);
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
