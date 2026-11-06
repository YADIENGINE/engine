#include "Fx/Particles/ParticleRecipes.h"
#include <algorithm>
#include <cmath>
#include "Fx/Internals/FxChecks.h"

namespace {
bool ValidLayer(const ParticleLayer& layer) { return ParticleSystem::IsValid(layer.emission, layer.appearance); }
bool ValidUse(const ParticleLayer& layer, int count) { return count >= 0 && (count == 0 || ValidLayer(layer)); }
} // namespace

bool IsValid(const ParticleBurst& burst) {
    return std::ranges::all_of(burst, [](const ParticleLayer& layer) {
        return layer.share <= 0.0f || (std::isfinite(layer.share) && ValidLayer(layer));
    });
}
bool IsValid(const ParticleRecipes& recipes) {
    return IsValid(recipes.soft) && IsValid(recipes.spark) && IsValid(recipes.sparkle) && IsValid(recipes.debris) &&
           IsValid(recipes.pixel) && IsValid(recipes.confetti) && IsValid(recipes.trail) && IsValid(recipes.smoke) &&
           IsValid(recipes.flame) && IsValid(recipes.embers) && ValidUse(recipes.dash, recipes.dashCount) &&
           ValidUse(recipes.steam, recipes.steamCount) && FxNotNegative(recipes.steamWhiten) &&
           FxNotNegative(recipes.steamOpacity);
}

ParticleLayer AimedBehind(ParticleLayer layer, Float2 direction) {
    layer.emission.direction = std::atan2(-direction.y, -direction.x);
    return layer;
}

void EmitLayer(ParticleSystem& particles, const ParticleSource& source, Color4 color, int count,
               const ParticleLayer& layer, const ParticleCustomDraw& draw) {
    if (count <= 0) return;
    particles.Emit(source, color, count, layer.emission, layer.appearance, draw);
}
void EmitBurst(ParticleSystem& particles, const ParticleSource& source, Color4 color, int count,
               const ParticleBurst& burst, const ParticleCustomDraw& draw) {
    if (count <= 0) return;
    for (const auto& layer : burst) {
        if (layer.share <= 0.0f) continue;
        EmitLayer(particles, source, color, static_cast<int>(std::lround(static_cast<float>(count) * layer.share)),
                  layer, draw);
    }
}
