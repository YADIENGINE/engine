#pragma once
#include <raylib.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <vector>
#include "Utilities/FloatRange.h"
#include "Graphics/Draw/ParticleDraw.h"

enum class ParticleBlend { Alpha, Additive };

struct ParticleEmission {
    FloatRange speed;
    FloatRange lifetime;
    FloatRange size;
    FloatRange rotation;    // degrees
    FloatRange spin;        // degrees/second
    float direction = 0.0f; // radians
    float spread = 2.0f * PI;
    Vector2 acceleration{}; // canvas pixels/second squared
    float drag = 0.0f;
};

struct ParticleAppearance {
    Color color = WHITE;
    ParticleShape shape = ParticleShape::Circle;
    ParticleBlend blend = ParticleBlend::Alpha;
    float endSizeScale = 1.0f;
    float sizeScale = 1.0f;
    float fadeFraction = 1.0f; // fraction of lifetime spent fading; zero disables fade
    float sparkTrailSeconds = 0.0f;
    float sparkThickness = 1.0f; // fraction of initial particle size
    float sparkMinThickness = 0.0f;
    // Draw only: do not mutate this system or change blend/camera/scissor/target state.
    // Captured texture handles/references must outlive the emitted particles.
    std::function<void(const ParticleDrawData&)> draw{};
};

struct ParticleSystemSettings {
    std::size_t capacity;
    std::optional<std::uint32_t> seed{};
};

class ParticleSystem2D {
public:
    explicit ParticleSystem2D(const ParticleSystemSettings& settings);
    void Emit(Vector2 position, int count, const ParticleEmission& emission, const ParticleAppearance& appearance);
    void Update(float deltaSeconds);
    void Clear();
    // Alpha pass followed by additive pass; returns to alpha blending, leaves clipping/camera to caller.
    void Draw() const;
    [[nodiscard]] std::size_t ActiveCount() const { return _particles.size(); }

private:
    struct Particle {
        Vector2 position;
        Vector2 velocity;
        float life;
        float duration;
        float size;
        float rotation;
        float spin;
        Vector2 acceleration;
        float drag;
        ParticleAppearance appearance;
    };
    void DrawPass(ParticleBlend blend) const;
    std::size_t _capacity;
    std::vector<Particle> _particles;
    std::mt19937 _rng;
};
