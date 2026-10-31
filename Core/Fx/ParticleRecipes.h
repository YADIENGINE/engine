#pragma once
#include <array>
#include "Fx/ParticleSystem2D.h"

// Ready-made particle bursts. Soft: soft circle + spark; Spark: spark; Sparkle: rising star; Debris: falling square
// + spark; Pixel: falling pixel; Confetti: drifting square.
enum class ParticleRecipe { Soft, Spark, Sparkle, Debris, Pixel, Confetti };

// Shared recipe dimensions; the theme supplies appearance (Theme::fx.particles). Speed factors scale emission.speed,
// gravities are downward acceleration (negative: rising), ratios give secondary-layer counts relative to the main
// layer.
struct ParticleRecipeTuning {
    ParticleEmission emission; // Shared emission of burst layers
    float endSizeScale = 1.0f;
    float sparkTrailSeconds = 0.0f;
    float sparkThickness = 1.0f;
    float sparkMinThickness = 0.0f;
    float starScale = 1.0f;
    float pixelScale = 1.0f;
    float sparkSpeed = 1.0f;
    float softSparkRatio = 0.0f;
    float sparkleSpeed = 1.0f;
    float sparkleGravity = 0.0f;
    float sparkleSoftRatio = 0.0f;
    float debrisSpeed = 1.0f;
    float debrisGravity = 0.0f;
    float debrisSparkRatio = 0.0f;
    float pixelGravity = 0.0f;
    float confettiSpeed = 1.0f;
    float confettiGravity = 0.0f;
    int trailCount = 0; // Trail: light left behind a moving object
    float trailSpeed = 1.0f;
    int dashCount = 0; // Dash: sparks opposite the facing direction
    float dashSpeed = 1.0f;
    float dashSpread = 0.0f;
    int steamCount = 0; // Steam: rising haze that grows and fades
    ParticleEmission steam;
    float steamEndSizeScale = 1.0f;
    float steamWhiten = 0.0f;
    float steamOpacity = 1.0f;
};

// One emission layer; 'share' is the layer's portion of the requested burst count.
struct ParticleLayer {
    float share = 0.0f;
    ParticleEmission emission;
    ParticleAppearance appearance;
};
using ParticleBurst = std::array<ParticleLayer, 2>;

[[nodiscard]] ParticleBurst BurstRecipe(ParticleRecipe recipe, const ParticleRecipeTuning& tuning);
[[nodiscard]] ParticleLayer TrailRecipe(const ParticleRecipeTuning& tuning);
// 'direction': movement direction; sparks scatter in the opposite direction.
[[nodiscard]] ParticleLayer DashRecipe(Float2 direction, const ParticleRecipeTuning& tuning);
[[nodiscard]] ParticleLayer SteamRecipe(const ParticleRecipeTuning& tuning);

// Emits 'count' particles of the layer in 'color'.
void EmitLayer(ParticleSystem2D& particles, Float2 position, Color4 color, int count, const ParticleLayer& layer);
// Distributes 'count' among the recipe's layers according to their shares.
void EmitBurst(ParticleSystem2D& particles, Float2 position, Color4 color, int count, ParticleRecipe recipe,
               const ParticleRecipeTuning& tuning);
