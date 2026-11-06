#include "Fx/Particles/ParticleEmitter.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
bool Rate(float value) { return std::isfinite(value) && value >= 0.0f; }

const ParticleEmitterStyle& Checked(const ParticleEmitterStyle& style) {
    if (!IsValid(style)) throw std::invalid_argument("Invalid particle emitter style");
    return style;
}
} // namespace

bool IsValid(const ParticleEmitterStyle& style) {
    if (!Rate(style.perSecond) || !Rate(style.perUnit)) return false;
    const bool emits = style.perSecond > 0.0f || style.perUnit > 0.0f;
    return !emits || ParticleSystem::IsValid(style.emission, style.appearance);
}

ParticleEmitter::ParticleEmitter(const ParticleEmitterStyle& style)
    : _style(Checked(style)) {}

void ParticleEmitter::SetStyle(const ParticleEmitterStyle& style) { _style = Checked(style); }

void ParticleEmitter::Emit(ParticleSystem& particles, Float2 at, Color4 color, float seconds) {
    if (!(seconds >= 0.0f) || !std::isfinite(seconds) || !std::isfinite(at.x) || !std::isfinite(at.y)) return;
    const Float2 from = _last.value_or(at);
    _last = at;
    const double share = std::clamp(particles.Quality().particlePercent, 0, 100) / 100.0;
    const double amount =
        (static_cast<double>(_style.perSecond) * seconds + static_cast<double>(_style.perUnit) * Distance(from, at)) *
        share;
    if (!(amount > 0.0)) return;
    // The k-th particle (from 0) is born when the owed share reaches k + 1.
    const double total = _owed + amount;
    const double whole = std::floor(total);
    // More than the system holds would only replace each other: the newest, nearest the source, are kept.
    const double kept = std::min(whole, static_cast<double>(particles.Capacity()));
    const auto first = static_cast<float>((1.0 + (whole - kept) - _owed) / amount);
    const auto step = static_cast<float>(1.0 / amount);
    _owed = total - whole;
    const int count = static_cast<int>(kept);
    if (count == 0) return;
    const Float2 velocity = seconds > 0.0f ? (at - from) / seconds : Float2{};
    particles.Stream({at, velocity}, color, count, _style.emission, _style.appearance, from, seconds, first, step);
}
