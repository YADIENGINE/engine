#include "Fx/ParticleRecipes.h"
#include <cmath>

namespace {
ParticleLayer Layer(const ParticleRecipeTuning& tuning, float share, ParticleShape shape, float speedScale,
                    float gravity, ParticleBlend blend) {
    ParticleLayer layer{.share = share, .emission = tuning.emission};
    layer.emission.speed = {tuning.emission.speed.min * speedScale, tuning.emission.speed.max * speedScale};
    layer.emission.acceleration = {0.0f, gravity};
    layer.appearance = {.shape = shape,
                        .blend = blend,
                        .endSizeScale = tuning.endSizeScale,
                        .sizeScale = shape == ParticleShape::Star    ? tuning.starScale
                                     : shape == ParticleShape::Pixel ? tuning.pixelScale
                                                                     : 1.0f,
                        .sparkTrailSeconds = tuning.sparkTrailSeconds,
                        .sparkThickness = tuning.sparkThickness,
                        .sparkMinThickness = tuning.sparkMinThickness};
    return layer;
}
} // namespace

ParticleBurst BurstRecipe(ParticleRecipe recipe, const ParticleRecipeTuning& tuning) {
    using enum ParticleShape;
    using enum ParticleBlend;
    const auto& t = tuning;
    switch (recipe) {
    case ParticleRecipe::Soft:
        return {Layer(t, 1.0f, Circle, 1.0f, 0.0f, Alpha),
                Layer(t, t.softSparkRatio, Spark, t.sparkSpeed, 0.0f, Additive)};
    case ParticleRecipe::Spark: return {Layer(t, 1.0f, Spark, t.sparkSpeed, 0.0f, Additive), {}};
    case ParticleRecipe::Sparkle:
        return {Layer(t, 1.0f, Star, t.sparkleSpeed, t.sparkleGravity, Additive),
                Layer(t, t.sparkleSoftRatio, Circle, 1.0f, 0.0f, Alpha)};
    case ParticleRecipe::Debris:
        return {Layer(t, 1.0f, Square, t.debrisSpeed, t.debrisGravity, Alpha),
                Layer(t, t.debrisSparkRatio, Spark, t.sparkSpeed, 0.0f, Additive)};
    case ParticleRecipe::Pixel   : return {Layer(t, 1.0f, Pixel, 1.0f, t.pixelGravity, Alpha), {}};
    case ParticleRecipe::Confetti: return {Layer(t, 1.0f, Square, t.confettiSpeed, t.confettiGravity, Alpha), {}};
    }
    return {};
}
ParticleLayer TrailRecipe(const ParticleRecipeTuning& tuning) {
    return Layer(tuning, 1.0f, ParticleShape::Circle, tuning.trailSpeed, 0.0f, ParticleBlend::Additive);
}
ParticleLayer DashRecipe(Float2 direction, const ParticleRecipeTuning& tuning) {
    auto layer = Layer(tuning, 1.0f, ParticleShape::Spark, tuning.dashSpeed, 0.0f, ParticleBlend::Additive);
    layer.emission.direction = std::atan2(-direction.y, -direction.x);
    layer.emission.spread = tuning.dashSpread;
    return layer;
}
ParticleLayer SteamRecipe(const ParticleRecipeTuning& tuning) {
    return {.share = 1.0f,
            .emission = tuning.steam,
            .appearance = {.blend = ParticleBlend::Additive, .endSizeScale = tuning.steamEndSizeScale}};
}

void EmitLayer(ParticleSystem2D& particles, Float2 position, Color4 color, int count, const ParticleLayer& layer) {
    if (count <= 0) return;
    auto appearance = layer.appearance;
    appearance.color = color;
    particles.Emit(position, count, layer.emission, appearance);
}
void EmitBurst(ParticleSystem2D& particles, Float2 position, Color4 color, int count, ParticleRecipe recipe,
               const ParticleRecipeTuning& tuning) {
    if (count <= 0) return;
    for (const auto& layer : BurstRecipe(recipe, tuning)) {
        if (layer.share <= 0.0f) continue;
        EmitLayer(particles, position, color, static_cast<int>(std::lround(static_cast<float>(count) * layer.share)),
                  layer);
    }
}
