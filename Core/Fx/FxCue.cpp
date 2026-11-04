#include "Fx/FxCue.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include "Fx/Internals/FxChecks.h"

namespace {
bool Valid(const FxAction& action) {
    return std::visit(
        [](const auto& a) {
            using A = std::decay_t<decltype(a)>;
            if constexpr (std::same_as<A, FxBurst>)
                return a.count >= 0;
            else if constexpr (std::same_as<A, FxParticles>)
                return a.count >= 0 && IsValid(a.burst);
            else if constexpr (std::same_as<A, FxPopText>)
                return FxPositive(a.size);
            else if constexpr (std::same_as<A, FxShockwave>)
                return FxNotNegative(a.reach);
            else if constexpr (std::same_as<A, FxPunch>)
                return FxNotNegative(a.amount);
            else if constexpr (std::same_as<A, FxKick>)
                return std::isfinite(a.strength);
            else if constexpr (std::same_as<A, FxShake>)
                return FxPositive(a.amount);
            else if constexpr (std::same_as<A, FxGlitch> || std::same_as<A, FxHitstop>)
                return FxPositive(a.seconds);
            else if constexpr (std::same_as<A, FxSlowMotion>)
                return FxPositive(a.seconds) && FxNotNegative(a.startScale) && FxNotNegative(a.endScale);
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
        if (!FxNotNegative(entry.at)) throw std::invalid_argument("A cue action has an invalid time");
        if (!Valid(entry.action)) throw std::invalid_argument("A cue action has invalid values");
        if (std::holds_alternative<FxHitstop>(entry.action) || std::holds_alternative<FxSlowMotion>(entry.action))
            _usesTime = true;
    }
    std::ranges::stable_sort(_entries, {}, &FxCueEntry::at);
}
