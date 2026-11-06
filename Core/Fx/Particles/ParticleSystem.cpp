#include "Fx/Particles/ParticleSystem.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Animation/Easing.h"
#include "Graphics/Paint/Painter.h"

namespace {
bool Finite(float value) { return std::isfinite(value); }
bool NotNegative(float value) { return std::isfinite(value) && value >= 0.0f; }
bool ValidRange(FloatRange range) { return Finite(range.min) && Finite(range.max) && range.min <= range.max; }

bool ValidArea(const ParticleArea& area) {
    if (!NotNegative(area.radius) || !NotNegative(area.innerRadius) || area.innerRadius > area.radius ||
        !NotNegative(area.halfSize.x) || !NotNegative(area.halfSize.y) || !NotNegative(area.length) ||
        !Finite(area.angle))
        return false;
    return !(area.outward && area.shape == ParticleAreaShape::Point);
}

bool ValidColors(const ParticleColors& colors, bool own) {
    if (colors.count < 0 || colors.count > static_cast<int>(MaxParticleColorKeys)) return false;
    if (own && colors.count == 0) return false; // Its own colours need some
    float previous = 0.0f;
    for (int i = 0; i < colors.count; ++i) {
        const float at = colors.keys[static_cast<std::size_t>(i)].at;
        if (!Finite(at) || at < previous || at > 1.0f) return false;
        previous = at;
    }
    return true;
}

float Sample(Rng& rng, FloatRange range) { return rng.Float(range.min, range.max); }

// A point of the area around its centre; a Point draws nothing, so its sequence stays as it was.
Float2 AreaOffset(Rng& rng, const ParticleArea& area) {
    switch (area.shape) {
    case ParticleAreaShape::Point: return {};
    case ParticleAreaShape::Disc : {
        const float angle = rng.Float(0.0f, 2.0f * Pi);
        const float radius = area.radius * std::sqrt(rng.Float01());
        return {std::cos(angle) * radius, std::sin(angle) * radius};
    }
    case ParticleAreaShape::Ring: {
        const float angle = rng.Float(0.0f, 2.0f * Pi);
        const float inner = area.innerRadius * area.innerRadius;
        const float radius = std::sqrt(inner + (area.radius * area.radius - inner) * rng.Float01());
        return {std::cos(angle) * radius, std::sin(angle) * radius};
    }
    case ParticleAreaShape::Box: {
        const Float2 local{rng.Float(-area.halfSize.x, area.halfSize.x), rng.Float(-area.halfSize.y, area.halfSize.y)};
        return Rotated(local, area.angle);
    }
    case ParticleAreaShape::Line: return Rotated({rng.Float(-0.5f, 0.5f) * area.length, 0.0f}, area.angle);
    }
    return {};
}

// Mix without its clamp and lround: 't' is already 0..1 here, and this runs for every particle drawn.
Color4 Between(Color4 from, Color4 to, float t) {
    const auto channel = [t](std::uint8_t a, std::uint8_t b) {
        const auto x = static_cast<float>(a);
        return static_cast<std::uint8_t>(x + (static_cast<float>(b) - x) * t + 0.5f);
    };
    return {channel(from.r, to.r), channel(from.g, to.g), channel(from.b, to.b), channel(from.a, to.a)};
}

Color4 KeysAt(const ParticleColors& colors, float age) {
    const auto count = static_cast<std::size_t>(std::clamp(colors.count, 0, static_cast<int>(MaxParticleColorKeys)));
    if (count == 0) return ParticleWhite;
    const auto& keys = colors.keys;
    if (age <= keys[0].at) return keys[0].color;
    for (std::size_t i = 1; i < count; ++i) {
        if (age >= keys[i].at) continue;
        const float span = keys[i].at - keys[i - 1].at;
        return span > 0.0f ? Between(keys[i - 1].color, keys[i].color, (age - keys[i - 1].at) / span) : keys[i].color;
    }
    return keys[count - 1].color;
}

std::uint8_t Times(std::uint8_t a, std::uint8_t b) {
    return static_cast<std::uint8_t>((static_cast<unsigned>(a) * b + 127U) / 255U);
}
Color4 Multiply(Color4 a, Color4 b) { return {Times(a.r, b.r), Times(a.g, b.g), Times(a.b, b.b), Times(a.a, b.a)}; }
} // namespace

Color4 ParticleColorAt(const ParticleAppearance& appearance, Color4 color, float age) {
    const Color4 key = KeysAt(appearance.colors, age);
    return appearance.ownColors ? key : Multiply(color, key);
}

ParticleSystem::ParticleSystem(const ParticleSystemSettings& settings)
    : _capacity(settings.capacity)
    , _rng(settings.seed) {
    _particles.reserve(_capacity);
}

bool ParticleSystem::IsValid(const ParticleEmission& emission, const ParticleAppearance& appearance) {
    return ValidRange(emission.speed) && ValidRange(emission.lifetime) && emission.lifetime.min > 0.0f &&
           ValidRange(emission.size) && emission.size.min >= 0.0f && ValidRange(emission.rotation) &&
           ValidRange(emission.spin) && Finite(emission.direction) && NotNegative(emission.spread) &&
           Finite(emission.acceleration.x) && Finite(emission.acceleration.y) && NotNegative(emission.drag) &&
           ValidArea(emission.area) && Finite(emission.inherit) &&
           ValidColors(appearance.colors, appearance.ownColors) && NotNegative(appearance.size.start) &&
           NotNegative(appearance.size.end) && NotNegative(appearance.sparkTrailSeconds) &&
           NotNegative(appearance.sparkThickness) && NotNegative(appearance.sparkMinThickness);
}

std::uint32_t ParticleSystem::LookFor(const ParticleEmission& emission, const ParticleAppearance& appearance,
                                      const ParticleCustomDraw& draw) {
    // A custom drawing cannot be compared, so it always gets an entry of its own.
    if (!draw) {
        for (std::size_t i = 0; i < _looks.size(); ++i) {
            const Look& look = _looks[i];
            if (look.alive > 0 && !look.draw && look.appearance == appearance &&
                look.acceleration == emission.acceleration && look.drag == emission.drag)
                return static_cast<std::uint32_t>(i);
        }
    }
    const Look fresh{appearance, emission.acceleration, emission.drag, draw, 0};
    const auto unused = std::ranges::find_if(_looks, [](const Look& look) { return look.alive == 0; });
    if (unused != _looks.end()) {
        *unused = fresh;
        return static_cast<std::uint32_t>(unused - _looks.begin());
    }
    _looks.push_back(fresh);
    return static_cast<std::uint32_t>(_looks.size() - 1);
}

void ParticleSystem::Forget(const Particle& particle) {
    Look& look = _looks[particle.look];
    if (--look.alive == 0) look.draw = {}; // Let go of what the drawing holds
}

std::size_t ParticleSystem::MakeRoom(int count) {
    const auto wanted = std::min(static_cast<std::size_t>(std::max(count, 0)), _capacity);
    const std::size_t room = _capacity - _particles.size();
    if (wanted > room) {
        const auto end = _particles.begin() + static_cast<std::ptrdiff_t>(wanted - room);
        for (auto it = _particles.begin(); it != end; ++it)
            Forget(*it);
        _particles.erase(_particles.begin(), end);
    }
    return wanted;
}

void ParticleSystem::Add(Float2 at, Float2 sourceVelocity, Color4 color, std::uint32_t look,
                         const ParticleEmission& emission, float age) {
    const Float2 offset = AreaOffset(_rng, emission.area);
    const float direction = emission.area.outward ? std::atan2(offset.y, offset.x) : emission.direction;
    const float angle = Sample(_rng, {direction - emission.spread / 2.0f, direction + emission.spread / 2.0f});
    const float speed = Sample(_rng, emission.speed);
    const float life = Sample(_rng, emission.lifetime);
    const float size = Sample(_rng, emission.size);
    const float rotation = Sample(_rng, emission.rotation);
    const float spin = Sample(_rng, emission.spin);
    if (age >= life) return; // Born and gone within the frame
    Particle particle{
        at + offset, Float2{std::cos(angle) * speed, std::sin(angle) * speed} + sourceVelocity * emission.inherit,
        life,        life,
        size,        rotation,
        spin,        color,
        look};
    if (age > 0.0f) {
        particle.velocity += emission.acceleration * age;
        particle.position += particle.velocity * age;
        particle.velocity *= DecayFactor(emission.drag, age);
        particle.rotation += particle.spin * age;
        particle.life -= age;
    }
    ++_looks[look].alive;
    _particles.push_back(particle);
}

void ParticleSystem::Emit(const ParticleSource& source, Color4 color, int count, const ParticleEmission& emission,
                          const ParticleAppearance& appearance, const ParticleCustomDraw& draw) {
    if (!IsValid(emission, appearance)) throw std::invalid_argument("Invalid particle emission or appearance");
    const std::size_t wanted = MakeRoom(_quality.ParticleCount(count));
    if (wanted == 0) return;
    const std::uint32_t look = LookFor(emission, appearance, draw);
    for (std::size_t i = 0; i < wanted; ++i)
        Add(source.at, source.velocity, color, look, emission, 0.0f);
}

void ParticleSystem::Stream(const ParticleSource& source, Color4 color, int count, const ParticleEmission& emission,
                            const ParticleAppearance& appearance, Float2 from, float seconds, float firstAlong,
                            float step) {
    if (!IsValid(emission, appearance)) throw std::invalid_argument("Invalid particle emission or appearance");
    const std::size_t wanted = MakeRoom(count);
    if (wanted == 0) return;
    const std::uint32_t look = LookFor(emission, appearance, {});
    // Fewer than asked fit: the newest, nearest the source, are the ones to keep.
    for (int k = count - static_cast<int>(wanted); k < count; ++k) {
        const float along = std::clamp(firstAlong + static_cast<float>(k) * step, 0.0f, 1.0f);
        Add(from + (source.at - from) * along, source.velocity, color, look, emission, (1.0f - along) * seconds);
    }
}

void ParticleSystem::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    for (Particle& particle : _particles) {
        const Look& look = _looks[particle.look];
        particle.velocity += look.acceleration * deltaSeconds;
        particle.position += particle.velocity * deltaSeconds;
        particle.velocity *= DecayFactor(look.drag, deltaSeconds);
        particle.rotation += particle.spin * deltaSeconds;
        particle.life -= deltaSeconds;
    }
    std::erase_if(_particles, [this](const Particle& particle) {
        if (particle.life > 0.0f) return false;
        Forget(particle);
        return true;
    });
}

void ParticleSystem::Clear() {
    _particles.clear();
    _looks.clear();
}

void ParticleSystem::DrawPass(Painter& paint, ParticleBlend blend) const {
    for (const Particle& particle : _particles) {
        const Look& look = _looks[particle.look];
        const ParticleAppearance& appearance = look.appearance;
        if (appearance.blend != blend) continue;
        const float age = 1.0f - particle.life / particle.duration;
        const float grow = Mix(appearance.size.start, appearance.size.end, Ease(appearance.size.curve, age));
        const ParticleDrawData data{.position = particle.position,
                                    .velocity = particle.velocity,
                                    .color = ParticleColorAt(appearance, particle.color, age),
                                    .size = particle.size * grow,
                                    .rotation = particle.rotation,
                                    .normalizedAge = age};
        if (look.draw) {
            look.draw(paint, data);
            continue;
        }
        DrawParticleShape(paint, appearance.shape, data,
                          {appearance.sparkTrailSeconds,
                           std::max(appearance.sparkMinThickness, particle.size * appearance.sparkThickness)});
    }
}

void ParticleSystem::Draw(Painter& paint) const {
    DrawPass(paint, ParticleBlend::Alpha);
    auto light = paint.Blending(Blend::Add);
    DrawPass(paint, ParticleBlend::Additive);
}
