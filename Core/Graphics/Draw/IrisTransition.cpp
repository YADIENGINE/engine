#include "Fx/IrisTransition.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Paint/Painter.h"

namespace {
constexpr float FullyCoveredRadius = 1.0f;
} // namespace

void DrawIrisTransition(Painter& paint, const FloatRect& bounds, Float2 focus, float cover, const IrisStyle& style) {
    if (bounds.Empty()) return;
    cover = std::clamp(cover, 0.0f, 1.0f);
    if (cover == 0.0f) return;
    const float glowWidth = std::max(0.0f, style.glowWidth);
    const float rimWidth = std::max(0.0f, style.rimWidth);
    const float reach =
        std::max({Distance(focus, bounds.Position()), Distance(focus, {bounds.Right(), bounds.y}),
                  Distance(focus, {bounds.x, bounds.Bottom()}), Distance(focus, {bounds.Right(), bounds.Bottom()})}) +
        std::max(glowWidth, rimWidth);
    const float radius = reach * (1.0f - cover);
    auto clip = paint.Clip(bounds);
    if (radius < FullyCoveredRadius) {
        paint.Rect(bounds, Fill(style.curtain));
        return;
    }
    paint.Circle(focus, radius, Outline(reach + glowWidth - radius, style.curtain, StrokeAlign::Outside));
    if (glowWidth > 0.0f) {
        auto light = paint.Blending(Blend::Add);
        paint.Circle(focus, radius, Outline(std::min(glowWidth, radius), style.glow, StrokeAlign::Inside));
    }
    if (rimWidth > 0.0f) {
        paint.Circle(focus, radius, Outline(std::min(rimWidth, radius), style.innerRim, StrokeAlign::Inside));
        paint.Circle(focus, radius, Outline(rimWidth, style.outerRim, StrokeAlign::Outside));
    }
}
