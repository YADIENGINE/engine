#include "Graphics/Draw/GlowBatch.h"
#include "Graphics/Draw/EffectQuality.h"

GlowBatch::~GlowBatch() {
    if (_active) EndBlendMode();
}

void GlowBatch::Draw(Vector2 center, float radius, Color color) {
    if (CurrentEffectQuality().glow == EffectLevel::Off) return;
    if (!_active) {
        BeginBlendMode(BLEND_ADDITIVE);
        _active = true;
    }
    DrawCircleGradient(static_cast<int>(center.x), static_cast<int>(center.y), radius, color, Fade(color, 0.0f));
}
