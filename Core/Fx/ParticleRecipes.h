#pragma once
#include <array>
#include "Fx/ParticleSystem.h"

// Ready-made particle bursts. Soft: soft circle + spark; Spark: spark; Sparkle: rising star; Debris: falling square
// + spark; Pixel: falling pixel; Confetti: drifting square.
enum class ParticleRecipe { Soft, Spark, Sparkle, Debris, Pixel, Confetti };

// One emission layer; 'share' is the layer's portion of the requested burst count. A share of 0 is skipped.
struct ParticleLayer {
    float share = 0.0f;
    ParticleEmission emission;
    ParticleAppearance appearance;
};
using ParticleBurst = std::array<ParticleLayer, 2>;

// What each recipe looks like, as plain data the theme supplies (Theme::fx.particles). A game's own burst is the same
// data: give any ParticleBurst to EmitBurst or FxStage::Burst.
struct ParticleRecipes {
    ParticleBurst soft{};
    ParticleBurst spark{};
    ParticleBurst sparkle{};
    ParticleBurst debris{};
    ParticleBurst pixel{};
    ParticleBurst confetti{};
    ParticleLayer trail{}; // Light left behind a moving object
    int trailCount = 0;
    ParticleLayer dash{}; // Sparks opposite the facing direction; AimedBehind sets its direction
    int dashCount = 0;
    ParticleLayer steam{}; // Rising haze that grows and fades
    int steamCount = 0;
    float steamWhiten = 0.0f;
    float steamOpacity = 1.0f;

    [[nodiscard]] constexpr const ParticleBurst& Burst(ParticleRecipe recipe) const {
        switch (recipe) {
        case ParticleRecipe::Soft    : return soft;
        case ParticleRecipe::Spark   : return spark;
        case ParticleRecipe::Sparkle : return sparkle;
        case ParticleRecipe::Debris  : return debris;
        case ParticleRecipe::Pixel   : return pixel;
        case ParticleRecipe::Confetti: return confetti;
        }
        return soft;
    }
};

// The layer pointing away from 'direction', the way something moving leaves sparks behind it.
[[nodiscard]] ParticleLayer AimedBehind(ParticleLayer layer, Float2 direction);

// Emits 'count' particles of the layer in 'color'.
void EmitLayer(ParticleSystem& particles, Float2 position, Color4 color, int count, const ParticleLayer& layer);
// Distributes 'count' among the burst's layers according to their shares.
void EmitBurst(ParticleSystem& particles, Float2 position, Color4 color, int count, const ParticleBurst& burst);
