#include "Fx/ShockwaveEffects2D.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Anim/Easing.h"
#include "Graphics/Paint/Painter.h"

ShockwaveEffects2D::ShockwaveEffects2D(const ShockwaveStyle& style)
    : _style(style) {}

void ShockwaveEffects2D::Emit(Float2 center, Color4 color, float reach) { _waves.push_back({center, color, reach}); }
void ShockwaveEffects2D::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    for (Wave& wave : _waves)
        wave.age += deltaSeconds;
    std::erase_if(_waves, [this](const Wave& wave) { return wave.age >= _style.duration + _style.trailDelay; });
}
void ShockwaveEffects2D::Clear() { _waves.clear(); }

void ShockwaveEffects2D::Draw(Painter& paint, const FloatRect& bounds) const {
    if (_waves.empty()) return;
    const float arena = std::hypot(bounds.width, bounds.height);
    auto light = paint.Blending(Blend::Add);
    for (const Wave& ring : _waves) {
        const float maxRadius = ring.reach > 0.0f ? ring.reach : arena;
        for (int wave = 0; wave < 2; ++wave) {
            const float t = (ring.age - static_cast<float>(wave) * _style.trailDelay) / _style.duration;
            if (t <= 0.0f || t >= 1.0f) continue;
            const float radius = maxRadius * EaseOutCubic(t);
            const float width = _style.width * (1.0f - t) * (wave == 0 ? 1.0f : _style.trailWidth);
            const float alpha = (1.0f - t) * (wave == 0 ? _style.opacity : _style.trailOpacity);
            paint.Circle(ring.center, radius,
                         {.stroke = {std::min(width, radius), WithAlpha(ring.color, alpha), StrokeAlign::Inside}});
            if (_style.rimWidth > 0.0f)
                paint.Circle(ring.center, radius,
                             Outline(_style.rimWidth, WithAlpha(_style.rim, alpha), StrokeAlign::Outside));
        }
    }
}
