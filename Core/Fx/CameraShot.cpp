#include "Fx/CameraShot.h"
#include <algorithm>
#include "Graphics/Anim/Easing.h"
#include "Utilities/MathUtils.h"

CameraShot FrameCameraShot(const FloatRect& bounds, const CameraFraming& framing) {
    if (bounds.Empty()) return {};
    const float left = std::max(framing.viewport.x, framing.safeArea.x) + framing.padding;
    const float right = std::min(framing.viewport.Right(), framing.safeArea.Right()) - framing.padding;
    const float top = std::max(framing.viewport.y, framing.safeArea.y) + framing.padding;
    const float bottom = std::min(framing.viewport.Bottom(), framing.safeArea.Bottom()) - framing.padding;
    const float zoom = std::clamp(std::min((right - left) / bounds.width, (bottom - top) / bounds.height),
                                  framing.minZoom, framing.maxZoom);
    // Kamera görüntü alanının merkezini (offset) odağa koyar; kadrajın merkezi güvenli alanın merkezine düşsün.
    const Float2 center = framing.viewport.Center();
    const Float2 safe{(left + right) / 2.0f, (top + bottom) / 2.0f};
    return {.focus = bounds.Center() - (safe - center) / zoom, .zoom = zoom, .weight = 1.0f, .keepInArena = false};
}

CameraShot BlendShots(const CameraShot& from, const CameraShot& to, float t) {
    const auto mix = [t](float a, float b) { return a + (b - a) * t; };
    return {.focus = Mix(from.focus, to.focus, t),
            .zoom = mix(from.zoom, to.zoom),
            .rotation = mix(from.rotation, to.rotation),
            .weight = mix(from.weight, to.weight),
            .keepInArena = t < 0.5f ? from.keepInArena : to.keepInArena};
}

CameraView ShotCamera(const CameraShot& shot, const FloatRect& viewport, const FloatRect& arena) {
    const Float2 center = viewport.Center();
    const float weight = Clamp01(shot.weight);
    const float zoom = 1.0f + (shot.zoom - 1.0f) * weight;
    Float2 target = center + (shot.focus - center) * weight;
    if (zoom > 1.0f && shot.keepInArena) {
        const float halfWidth = viewport.width / (2.0f * zoom);
        const float halfHeight = viewport.height / (2.0f * zoom);
        target.x = std::clamp(target.x, arena.x + halfWidth, arena.Right() - halfWidth);
        target.y = std::clamp(target.y, arena.y + halfHeight, arena.Bottom() - halfHeight);
    }
    return {.offset = center, .target = target, .rotation = shot.rotation * weight, .zoom = zoom};
}
