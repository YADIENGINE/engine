#include "Fx/World/Rings.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Fx/Internals/FxChecks.h"
#include "Graphics/Paint/Painter.h"

bool IsValid(const RingStyle& style) {
    return FxPositive(style.duration) && FxNotNegative(style.startRadius) && FxNotNegative(style.endRadius) &&
           FxNotNegative(style.thickness) && FxNotNegative(style.fadeSeconds);
}

void Rings::Emit(Float2 position, Color4 color, const RingStyle& style) {
    if (!IsValid(style)) throw std::invalid_argument("Invalid ring style");
    _rings.push_back({position, color, style.duration, style});
}

void Rings::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    for (Ring& ring : _rings)
        ring.life -= deltaSeconds;
    std::erase_if(_rings, [](const Ring& ring) { return ring.life <= 0.0f; });
}

void Rings::Clear() { _rings.clear(); }

void Rings::Draw(Painter& paint) const {
    for (const Ring& ring : _rings) {
        if (ring.style.thickness <= 0.0f) continue;
        const float progress = Ease(ring.style.curve, 1.0f - ring.life / ring.style.duration);
        const float radius = ring.style.startRadius + (ring.style.endRadius - ring.style.startRadius) * progress;
        const float alpha = ring.style.fadeSeconds > 0.0f ? std::min(1.0f, ring.life / ring.style.fadeSeconds) : 1.0f;
        paint.Circle(
            ring.position, radius,
            Outline(std::min(ring.style.thickness, radius), WithAlpha(ring.color, alpha), StrokeAlign::Inside));
    }
}
