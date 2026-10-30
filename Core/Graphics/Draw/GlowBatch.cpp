#include "Graphics/Draw/GlowBatch.h"
#include "Graphics/Draw/EffectQuality.h"
#include "Graphics/Draw/DrawUtils.h"
#include "Graphics/Render/Internals/RenderExecutionContext.h"

GlowBatch::GlowBatch()
    : _individual(RenderExecutionContext::Glows() == GlowBatching::Individual) {}

GlowBatch::~GlowBatch() {
    if (_active) {
        EndBlendMode();
        RenderExecutionContext::Count(RenderCounter::GlowBlendRequests, 1);
    }
}

void GlowBatch::Draw(Vector2 center, float radius, Color color) {
    if (_individual) {
        DrawGlow(center, radius, color);
        return;
    }
    if (CurrentEffectQuality().glow == EffectLevel::Off) return;
    if (!_active) {
        BeginBlendMode(BLEND_ADDITIVE);
        _active = true;
        RenderExecutionContext::Count(RenderCounter::GlowBlendRequests, 1);
    }
    DrawCircleGradient(static_cast<int>(center.x), static_cast<int>(center.y), radius, color, Fade(color, 0.0f));
    RenderExecutionContext::Count(RenderCounter::Glows, 1);
}
