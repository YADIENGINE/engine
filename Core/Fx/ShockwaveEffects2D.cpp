#include "Fx/ShockwaveEffects2D.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Draw/DrawUtils.h"
#include "Graphics/Anim/Easing.h"
#include "Utilities/MathUtils.h"

ShockwaveEffects2D::ShockwaveEffects2D(const ShockwaveStyle& style)
    : _style(style) {}

void ShockwaveEffects2D::Emit(Vector2 center, Color color, float reach) { _waves.push_back({center, color, reach}); }
void ShockwaveEffects2D::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    for (Wave& wave : _waves)
        wave.age += deltaSeconds;
    std::erase_if(_waves, [this](const Wave& wave) { return wave.age >= _style.duration + _style.trailDelay; });
}
void ShockwaveEffects2D::Clear() { _waves.clear(); }

void ShockwaveEffects2D::Draw(const Rect& bounds) const {
    const float arena = std::hypot(static_cast<float>(bounds.width), static_cast<float>(bounds.height));
    BeginBlendMode(BLEND_ADDITIVE);
    for (const Wave& ring : _waves) {
        const float maxRadius = ring.reach > 0.0f ? ring.reach : arena;
        for (int wave = 0; wave < 2; ++wave) {
            const float t = (ring.age - static_cast<float>(wave) * _style.trailDelay) / _style.duration;
            if (t <= 0.0f || t >= 1.0f) continue;
            const float radius = maxRadius * EaseOutCubic(t);
            const float width = _style.width * (1.0f - t) * (wave == 0 ? 1.0f : _style.trailWidth);
            const float alpha = (1.0f - t) * (wave == 0 ? _style.opacity : _style.trailOpacity);
            DrawRing(ring.center, std::max(radius - width, 0.0f), radius, 0.0f, 360.0f, _style.segments,
                     Fade(ring.color, alpha));
            DrawRing(ring.center, radius, radius + _style.rimWidth, 0.0f, 360.0f, _style.segments,
                     Fade(_style.rim, alpha));
        }
    }
    EndBlendMode();
}
