#include "Fx/Shockwaves.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Graphics/Paint/Painter.h"

void Shockwaves::Emit(Float2 center, Color4 color, const ShockwaveStyle& style, float reach) {
    if (!std::isfinite(style.duration) || style.duration <= 0.0f) {
        throw std::invalid_argument("Invalid shockwave style");
    }
    for (const float value :
         {style.trailDelay, style.width, style.trailWidth, style.opacity, style.trailOpacity, style.rimWidth, reach}) {
        if (!std::isfinite(value) || value < 0.0f) throw std::invalid_argument("Invalid shockwave style");
    }
    _waves.push_back({center, color, reach, style});
}
void Shockwaves::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    for (Wave& wave : _waves)
        wave.age += deltaSeconds;
    std::erase_if(_waves, [](const Wave& wave) { return wave.age >= wave.style.duration + wave.style.trailDelay; });
}
void Shockwaves::Clear() { _waves.clear(); }

void Shockwaves::Draw(Painter& paint, const FloatRect& bounds) const {
    if (_waves.empty()) return;
    const float arena = std::hypot(bounds.width, bounds.height);
    auto light = paint.Blending(Blend::Add);
    for (const Wave& ring : _waves) {
        const ShockwaveStyle& style = ring.style;
        const float maxRadius = ring.reach > 0.0f ? ring.reach : arena;
        for (int wave = 0; wave < 2; ++wave) {
            const float t = (ring.age - static_cast<float>(wave) * style.trailDelay) / style.duration;
            if (t <= 0.0f || t >= 1.0f) continue;
            const float radius = maxRadius * Ease(style.curve, t);
            const float width = style.width * (1.0f - t) * (wave == 0 ? 1.0f : style.trailWidth);
            const float alpha = (1.0f - t) * (wave == 0 ? style.opacity : style.trailOpacity);
            paint.Circle(ring.center, radius,
                         {.stroke = {std::min(width, radius), WithAlpha(ring.color, alpha), StrokeAlign::Inside}});
            if (style.rimWidth > 0.0f)
                paint.Circle(ring.center, radius,
                             Outline(style.rimWidth, WithAlpha(style.rim, alpha), StrokeAlign::Outside));
        }
    }
}
