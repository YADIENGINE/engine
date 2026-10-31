#include "Fx/RingEffects2D.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Graphics/Paint/Painter.h"

void RingEffects2D::Emit(Float2 position, Color4 color, const RingStyle& style) {
    if (!std::isfinite(style.duration) || style.duration <= 0.0f || !std::isfinite(style.startRadius) ||
        style.startRadius < 0.0f || !std::isfinite(style.endRadius) || style.endRadius < 0.0f ||
        !std::isfinite(style.thickness) || style.thickness < 0.0f || !std::isfinite(style.fadeSeconds) ||
        style.fadeSeconds < 0.0f) {
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

void RingEffects2D::Draw(Painter& paint) const {
    for (const Ring& ring : _rings) {
        if (ring.style.thickness <= 0.0f) continue;
        const float progress = 1.0f - ring.life / ring.style.duration;
        const float radius = ring.style.startRadius + (ring.style.endRadius - ring.style.startRadius) * progress;
        const float alpha = ring.style.fadeSeconds > 0.0f ? std::min(1.0f, ring.life / ring.style.fadeSeconds) : 1.0f;
        paint.Circle(
            ring.position, radius,
            Outline(std::min(ring.style.thickness, radius), WithAlpha(ring.color, alpha), StrokeAlign::Inside));
    }
}
