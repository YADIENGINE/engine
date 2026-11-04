#include "Fx/Particles/ParticleRecipes.h"
#include <cmath>

ParticleLayer AimedBehind(ParticleLayer layer, Float2 direction) {
    layer.emission.direction = std::atan2(-direction.y, -direction.x);
    return layer;
}

void EmitLayer(ParticleSystem& particles, Float2 position, Color4 color, int count, const ParticleLayer& layer) {
    if (count <= 0) return;
    auto appearance = layer.appearance;
    appearance.color = color;
    particles.Emit(position, count, layer.emission, appearance);
}
void EmitBurst(ParticleSystem& particles, Float2 position, Color4 color, int count, const ParticleBurst& burst) {
    if (count <= 0) return;
    for (const auto& layer : burst) {
        if (layer.share <= 0.0f) continue;
        EmitLayer(particles, position, color, static_cast<int>(std::lround(static_cast<float>(count) * layer.share)),
                  layer);
    }
}
