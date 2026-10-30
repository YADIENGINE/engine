#include "Fx/ParticleSystem2D.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Graphics/Draw/EffectQuality.h"

namespace {
bool ValidRange(FloatRange range) {
    return std::isfinite(range.min) && std::isfinite(range.max) && range.min <= range.max;
}

float Sample(std::mt19937& rng, FloatRange range) {
    return std::uniform_real_distribution<float>(range.min, range.max)(rng);
}

void Validate(const ParticleEmission& emission, const ParticleAppearance& appearance) {
    if (!ValidRange(emission.speed) || !ValidRange(emission.lifetime) || emission.lifetime.min <= 0.0f ||
        !ValidRange(emission.size) || emission.size.min < 0.0f || !ValidRange(emission.rotation) ||
        !ValidRange(emission.spin) || !std::isfinite(emission.direction) || !std::isfinite(emission.spread) ||
        emission.spread < 0.0f || !std::isfinite(emission.acceleration.x) || !std::isfinite(emission.acceleration.y) ||
        !std::isfinite(emission.drag) || emission.drag < 0.0f || !std::isfinite(appearance.endSizeScale) ||
        appearance.endSizeScale < 0.0f || !std::isfinite(appearance.sizeScale) || appearance.sizeScale < 0.0f ||
        !std::isfinite(appearance.fadeFraction) || appearance.fadeFraction < 0.0f || appearance.fadeFraction > 1.0f ||
        !std::isfinite(appearance.sparkTrailSeconds) || appearance.sparkTrailSeconds < 0.0f ||
        !std::isfinite(appearance.sparkThickness) || appearance.sparkThickness < 0.0f ||
        !std::isfinite(appearance.sparkMinThickness) || appearance.sparkMinThickness < 0.0f) {
        throw std::invalid_argument("Invalid particle emission or appearance");
    }
}
} // namespace

ParticleSystem2D::ParticleSystem2D(const ParticleSystemSettings& settings)
    : _capacity(settings.capacity)
    , _rng(settings.seed ? *settings.seed : std::random_device{}()) {
    _particles.reserve(_capacity);
}

void ParticleSystem2D::Emit(Vector2 position, int count, const ParticleEmission& emission,
                            const ParticleAppearance& appearance) {
    Validate(emission, appearance);
    count = ScaledParticleCount(count);
    if (count <= 0) return;
    const auto available = std::min(static_cast<std::size_t>(count), _capacity - _particles.size());
    for (std::size_t i = 0; i < available; ++i) {
        const float angle =
            Sample(_rng, {emission.direction - emission.spread / 2.0f, emission.direction + emission.spread / 2.0f});
        const float speed = Sample(_rng, emission.speed);
        const float life = Sample(_rng, emission.lifetime);
        _particles.push_back({position,
                              {std::cos(angle) * speed, std::sin(angle) * speed},
                              life,
                              life,
                              Sample(_rng, emission.size),
                              Sample(_rng, emission.rotation),
                              Sample(_rng, emission.spin),
                              emission.acceleration,
                              emission.drag,
                              appearance});
    }
}

void ParticleSystem2D::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    for (Particle& particle : _particles) {
        particle.velocity.x += particle.acceleration.x * deltaSeconds;
        particle.velocity.y += particle.acceleration.y * deltaSeconds;
        particle.position.x += particle.velocity.x * deltaSeconds;
        particle.position.y += particle.velocity.y * deltaSeconds;
        const float drag = std::max(0.0f, 1.0f - particle.drag * deltaSeconds);
        particle.velocity.x *= drag;
        particle.velocity.y *= drag;
        particle.rotation += particle.spin * deltaSeconds;
        particle.life -= deltaSeconds;
    }
    std::erase_if(_particles, [](const Particle& particle) { return particle.life <= 0.0f; });
}

void ParticleSystem2D::Clear() { _particles.clear(); }

void ParticleSystem2D::DrawPass(ParticleBlend blend) const {
    for (const Particle& particle : _particles) {
        const ParticleAppearance& appearance = particle.appearance;
        if (appearance.blend != blend) continue;
        const float remaining = particle.life / particle.duration;
        const float alpha = appearance.fadeFraction > 0.0f ? std::min(1.0f, remaining / appearance.fadeFraction) : 1.0f;
        const Color color = Fade(appearance.color, alpha);
        const float size = particle.size * (appearance.endSizeScale + (1.0f - appearance.endSizeScale) * remaining) *
                           appearance.sizeScale;
        if (appearance.draw) {
            appearance.draw({particle.position, particle.velocity, color, size, particle.rotation, 1.0f - remaining});
            continue;
        }
        DrawParticleShape(appearance.shape,
                          {particle.position, particle.velocity, color, size, particle.rotation, 1.0f - remaining},
                          {appearance.sparkTrailSeconds,
                           std::max(appearance.sparkMinThickness, particle.size * appearance.sparkThickness)});
    }
}

void ParticleSystem2D::Draw() const {
    DrawPass(ParticleBlend::Alpha);
    BeginBlendMode(BLEND_ADDITIVE);
    try {
        DrawPass(ParticleBlend::Additive);
    } catch (...) {
        EndBlendMode();
        throw;
    }
    EndBlendMode();
}
