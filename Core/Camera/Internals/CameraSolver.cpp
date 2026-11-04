#include "Camera/Internals/CameraSolver.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
// The least move of 'center' that keeps [low, low + size] inside the window [center - half, center + half]; centred
// on it when the window is smaller.
float KeepSpanInWindow(float center, float low, float size, float half) {
    if (size >= 2.0f * half) return low + size * 0.5f;
    return std::clamp(center, low + size - half, low + half);
}
} // namespace

CameraGroup GatherTargets(std::span<const CameraTarget> targets) {
    CameraGroup group;
    Float2 sum{};
    float weights = 0.0f;
    for (const auto& target : targets) {
        if (!(target.weight > 0.0f)) continue;
        sum += target.position * target.weight;
        weights += target.weight;
        const float radius = std::max(target.radius, 0.0f);
        const FloatRect circle{target.position.x - radius, target.position.y - radius, radius * 2.0f, radius * 2.0f};
        if (!group.any) {
            group.bounds = circle;
        } else {
            const float left = std::min(group.bounds.x, circle.x);
            const float top = std::min(group.bounds.y, circle.y);
            group.bounds = {left, top, std::max(group.bounds.Right(), circle.Right()) - left,
                            std::max(group.bounds.Bottom(), circle.Bottom()) - top};
        }
        group.any = true;
    }
    if (group.any) group.center = sum / weights;
    return group;
}

Float2 VisibleSize(const FloatRect& viewport, float zoom) { return viewport.Size() / zoom; }

Float2 ApplyDeadZone(Float2 goal, Float2 focus, Float2 halfZone) {
    return {std::clamp(goal.x, focus.x - halfZone.x, focus.x + halfZone.x),
            std::clamp(goal.y, focus.y - halfZone.y, focus.y + halfZone.y)};
}

float FitZoom(const FloatRect& bounds, const FloatRect& viewport, const CameraZoom& zoom) {
    const float roomX = std::max(viewport.width - 2.0f * zoom.padding, 0.0f);
    const float roomY = std::max(viewport.height - 2.0f * zoom.padding, 0.0f);
    constexpr float Unbounded = std::numeric_limits<float>::max();
    const float byWidth = bounds.width > 0.0f ? roomX / bounds.width : Unbounded;
    const float byHeight = bounds.height > 0.0f ? roomY / bounds.height : Unbounded;
    return std::clamp(std::min(byWidth, byHeight), zoom.min, zoom.max);
}

Float2 KeepInView(Float2 center, const FloatRect& bounds, Float2 visibleSize, float margin) {
    const float halfX = std::max(visibleSize.x * 0.5f - margin, 0.0f);
    const float halfY = std::max(visibleSize.y * 0.5f - margin, 0.0f);
    return {KeepSpanInWindow(center.x, bounds.x, bounds.width, halfX),
            KeepSpanInWindow(center.y, bounds.y, bounds.height, halfY)};
}

Float2 Confine(Float2 center, Float2 visibleSize, const FloatRect& limits) {
    const auto axis = [](float middle, float size, float low, float span) {
        if (size >= span) return low + span * 0.5f;
        return std::clamp(middle, low + size * 0.5f, low + span - size * 0.5f);
    };
    return {axis(center.x, visibleSize.x, limits.x, limits.width),
            axis(center.y, visibleSize.y, limits.y, limits.height)};
}

Float2 SnapToPixels(Float2 target, Float2 offset, float zoom) {
    // A world point p lands on offset + (p - target) * zoom: keeping offset - target * zoom whole moves the world by
    // whole canvas units only.
    const Float2 shift = offset - target * zoom;
    const Float2 whole{std::round(shift.x), std::round(shift.y)};
    return (offset - whole) / zoom;
}
