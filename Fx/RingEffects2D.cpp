#include "Fx/RingEffects2D.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

void RingEffects2D::Emit(Vector2 position, Color color, const RingStyle& style) {
    if (!std::isfinite(style.duration) || style.duration <= 0.0f || !std::isfinite(style.startRadius) ||
        style.startRadius < 0.0f || !std::isfinite(style.endRadius) || style.endRadius < 0.0f ||
        !std::isfinite(style.thickness) || style.thickness < 0.0f || !std::isfinite(style.fadeSeconds) ||
        style.fadeSeconds < 0.0f || style.segments < 3) {
        throw std::invalid_argument("Invalid ring style");
    }
    _rings.push_back({position, color, style.duration, style});
}

void RingEffects2D::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    for (Ring& ring : _rings)
        ring.life -= deltaSeconds;
    std::erase_if(_rings, [](const Ring& ring) { return ring.life <= 0.0f; });
}

void RingEffects2D::Clear() { _rings.clear(); }

void RingEffects2D::Draw() const {
    for (const Ring& ring : _rings) {
        if (ring.style.thickness <= 0.0f) continue;
        const float progress = 1.0f - ring.life / ring.style.duration;
        const float radius = ring.style.startRadius + (ring.style.endRadius - ring.style.startRadius) * progress;
        const float alpha = ring.style.fadeSeconds > 0.0f ? std::min(1.0f, ring.life / ring.style.fadeSeconds) : 1.0f;
        DrawRing(ring.position, std::max(0.0f, radius - ring.style.thickness), radius, 0.0f, 360.0f,
                 ring.style.segments, Fade(ring.color, alpha));
    }
}
