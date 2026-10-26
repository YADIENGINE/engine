#include "Fx/CameraShot.h"
#include <algorithm>
#include "Graphics/Anim/Easing.h"

CameraShot FrameCameraShot(Rectangle bounds, const CameraFraming& framing) {
    if (!(bounds.width > 0.0f) || !(bounds.height > 0.0f)) return {};
    const float left = std::max(framing.viewport.x, framing.safeArea.x) + framing.padding;
    const float right =
        std::min(framing.viewport.x + framing.viewport.width, framing.safeArea.x + framing.safeArea.width) -
        framing.padding;
    const float top = std::max(framing.viewport.y, framing.safeArea.y) + framing.padding;
    const float bottom =
        std::min(framing.viewport.y + framing.viewport.height, framing.safeArea.y + framing.safeArea.height) -
        framing.padding;
    const float zoom = std::clamp(std::min((right - left) / bounds.width, (bottom - top) / bounds.height),
                                  framing.minZoom, framing.maxZoom);
    // Kamera görüntü alanının merkezini (offset) odağa koyar; kadrajın merkezi güvenli alanın merkezine düşsün.
    const Vector2 center{framing.viewport.x + framing.viewport.width / 2.0f,
                         framing.viewport.y + framing.viewport.height / 2.0f};
    const Vector2 safe{(left + right) / 2.0f, (top + bottom) / 2.0f};
    const Vector2 middle{bounds.x + bounds.width / 2.0f, bounds.y + bounds.height / 2.0f};
    return {.focus = {middle.x - (safe.x - center.x) / zoom, middle.y - (safe.y - center.y) / zoom},
            .zoom = zoom,
            .weight = 1.0f,
            .keepInArena = false};
}

CameraShot BlendShots(const CameraShot& from, const CameraShot& to, float t) {
    const auto mix = [t](float a, float b) { return a + (b - a) * t; };
    return {.focus = {mix(from.focus.x, to.focus.x), mix(from.focus.y, to.focus.y)},
            .zoom = mix(from.zoom, to.zoom),
            .rotation = mix(from.rotation, to.rotation),
            .weight = mix(from.weight, to.weight),
            .keepInArena = t < 0.5f ? from.keepInArena : to.keepInArena};
}

Camera2D ShotCamera(const CameraShot& shot, Rectangle viewport, Rectangle arena) {
    const Vector2 center{viewport.x + viewport.width / 2.0f, viewport.y + viewport.height / 2.0f};
    const float weight = Clamp01(shot.weight);
    const float zoom = 1.0f + (shot.zoom - 1.0f) * weight;
    Vector2 target{center.x + (shot.focus.x - center.x) * weight, center.y + (shot.focus.y - center.y) * weight};
    if (zoom > 1.0f && shot.keepInArena) {
        const float halfWidth = viewport.width / (2.0f * zoom);
        const float halfHeight = viewport.height / (2.0f * zoom);
        target.x = std::clamp(target.x, arena.x + halfWidth, arena.x + arena.width - halfWidth);
        target.y = std::clamp(target.y, arena.y + halfHeight, arena.y + arena.height - halfHeight);
    }
    return {.offset = center, .target = target, .rotation = shot.rotation * weight, .zoom = zoom};
}
