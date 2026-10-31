#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <vector>
#include "Fx/FxQuality.h"
#include "Graphics/Draw/ParticleDraw.h"
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRange.h"
#include "Utilities/MathUtils.h"

class Painter;

enum class ParticleBlend { Alpha, Additive };

struct ParticleEmission {
    FloatRange speed;
    FloatRange lifetime;
    FloatRange size;
    FloatRange rotation;    // radians, clockwise
    FloatRange spin;        // radians/second, clockwise
    float direction = 0.0f; // radians, clockwise
    float spread = 2.0f * Pi;
    Float2 acceleration{}; // canvas units/second squared
    float drag = 0.0f;
};

struct ParticleAppearance {
    Color4 color{255, 255, 255, 255};
    ParticleShape shape = ParticleShape::Circle;
    ParticleBlend blend = ParticleBlend::Alpha;
    float endSizeScale = 1.0f;
    float sizeScale = 1.0f;
    float fadeFraction = 1.0f; // fraction of lifetime spent fading; zero disables fade
    float sparkTrailSeconds = 0.0f;
    float sparkThickness = 1.0f; // fraction of initial particle size
    float sparkMinThickness = 0.0f;
    // Custom drawing instead of 'shape' (the blend is already set). Draw only: do not change this system.
    // Captured texture handles/references must outlive the emitted particles.
    std::function<void(Painter&, const ParticleDrawData&)> draw{};
};

struct ParticleSystemSettings {
    std::size_t capacity;
    std::optional<std::uint32_t> seed{};
};

class ParticleSystem2D {
public:
    explicit ParticleSystem2D(const ParticleSystemSettings& settings);
    // Emit makes particlePercent of the requested particles.
    void SetQuality(const FxQuality& quality) { _quality = quality; }
    void Emit(Float2 position, int count, const ParticleEmission& emission, const ParticleAppearance& appearance);
    void Update(float deltaSeconds);
    void Clear();
    // Alpha particles first, then additive ones on top.
    void Draw(Painter& paint) const;
    [[nodiscard]] std::size_t ActiveCount() const { return _particles.size(); }

private:
    struct Particle {
        Float2 position;
        Float2 velocity;
        float life;
        float duration;
        float size;
        float rotation;
        float spin;
        Float2 acceleration;
        float drag;
        ParticleAppearance appearance;
    };
    void DrawPass(Painter& paint, ParticleBlend blend) const;
    std::size_t _capacity;
    FxQuality _quality;
    std::vector<Particle> _particles;
    std::mt19937 _rng;
};
