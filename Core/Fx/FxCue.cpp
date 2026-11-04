#include "Fx/FxCue.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>

namespace {
bool Positive(float value) { return std::isfinite(value) && value > 0.0f; }
bool NotNegative(float value) { return std::isfinite(value) && value >= 0.0f; }

bool ValidBurst(const ParticleBurst& burst, int count) {
    if (count < 0) return false;
    return std::ranges::all_of(burst, [](const ParticleLayer& layer) {
        return layer.share <= 0.0f ||
               (std::isfinite(layer.share) && ParticleSystem::IsValid(layer.emission, layer.appearance));
    });
}

bool Valid(const FxAction& action) {
    return std::visit(
        [](const auto& a) {
            using A = std::decay_t<decltype(a)>;
            if constexpr (std::same_as<A, FxBurst>)
                return a.count >= 0;
            else if constexpr (std::same_as<A, FxParticles>)
                return ValidBurst(a.burst, a.count);
            else if constexpr (std::same_as<A, FxPopText>)
                return Positive(a.size);
            else if constexpr (std::same_as<A, FxShockwave>)
                return NotNegative(a.reach);
            else if constexpr (std::same_as<A, FxPunch>)
                return NotNegative(a.amount);
            else if constexpr (std::same_as<A, FxKick>)
                return std::isfinite(a.strength);
            else if constexpr (std::same_as<A, FxShake> || std::same_as<A, FxGlitch> || std::same_as<A, FxHitstop>)
                return Positive(a.seconds);
            else if constexpr (std::same_as<A, FxSlowMotion>)
                return Positive(a.seconds) && NotNegative(a.startScale) && NotNegative(a.endScale);
            else
                return true;
        },
        action);
}
} // namespace

FxCue::FxCue(std::initializer_list<FxCueEntry> entries)
    : FxCue(std::vector<FxCueEntry>(entries)) {}
FxCue::FxCue(std::vector<FxCueEntry> entries)
    : _entries(std::move(entries)) {
    if (_entries.empty()) throw std::invalid_argument("A cue needs at least one action");
    for (const FxCueEntry& entry : _entries) {
        if (!NotNegative(entry.at)) throw std::invalid_argument("A cue action has an invalid time");
        if (!Valid(entry.action)) throw std::invalid_argument("A cue action has invalid values");
        if (std::holds_alternative<FxHitstop>(entry.action) || std::holds_alternative<FxSlowMotion>(entry.action))
            _usesTime = true;
    }
    std::ranges::stable_sort(_entries, {}, &FxCueEntry::at);
}
