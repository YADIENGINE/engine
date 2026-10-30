#include "Fx/IrisTransition.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Draw/DrawUtils.h"
#include "Graphics/Draw/VectorMath.h"

namespace {
constexpr int IrisSegments = 96;
constexpr float FullyCoveredRadius = 1.0f;
} // namespace

void DrawIrisTransition(const Rect& bounds, Vector2 focus, float cover, const IrisStyle& style) {
    if (bounds.width <= 0 || bounds.height <= 0) return;
    cover = std::clamp(cover, 0.0f, 1.0f);
    if (cover == 0.0f) return;
    const float left = static_cast<float>(bounds.x);
    const float top = static_cast<float>(bounds.y);
    const float right = static_cast<float>(bounds.Right());
    const float bottom = static_cast<float>(bounds.Bottom());
    const float glowWidth = std::max(0.0f, style.glowWidth);
    const float rimWidth = std::max(0.0f, style.rimWidth);
    const float reach = std::max({Distance(focus, {left, top}), Distance(focus, {right, top}),
                                  Distance(focus, {left, bottom}), Distance(focus, {right, bottom})}) +
                        std::max(glowWidth, rimWidth);
    const float radius = reach * (1.0f - cover);
    BeginClip(bounds.x, bounds.y, bounds.width, bounds.height);
    if (radius < FullyCoveredRadius) {
        FillRect(bounds, style.curtain);
    } else {
        DrawRing(focus, radius, reach + glowWidth, 0.0f, 360.0f, IrisSegments, style.curtain);
        if (glowWidth > 0.0f) {
            BeginBlendMode(BLEND_ADDITIVE);
            DrawRing(focus, std::max(0.0f, radius - glowWidth), radius, 0.0f, 360.0f, IrisSegments, style.glow);
            EndBlendMode();
        }
        if (rimWidth > 0.0f) {
            DrawRing(focus, std::max(0.0f, radius - rimWidth), radius, 0.0f, 360.0f, IrisSegments, style.innerRim);
            DrawRing(focus, radius, radius + rimWidth, 0.0f, 360.0f, IrisSegments, style.outerRim);
        }
    }
    EndScissorMode();
}
