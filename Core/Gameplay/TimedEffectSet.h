#pragma once
#include <algorithm>
#include <optional>
#include <vector>

// Timed and permanent effects (buffs, debuffs, statuses): reacquiring a timed effect adds its duration to the
// remaining time and increases its stack; permanent effects stack as separate copies. Insertion order is preserved.
// The game defines each Id's behavior (including stack strength); this class only stores times and stacks. Header-only.
template <class Id> struct ActiveEffect {
    Id id;
    float duration = 0.0f;          // Full remaining-time bar (grows with remaining time on addition); 0 for permanent
    std::optional<float> remaining; // nullopt: permanent
    int stacks = 1;                 // Times a timed effect was acquired (up to 'maxStacks'); 1 for permanent
};

template <class Id> class TimedEffectSet {
public:
    // Timed effect: if active, adds 'duration' to remaining time and stacks up to 'maxStacks'; otherwise appends it.
    void AddTimed(Id id, float duration, int maxStacks) {
        const auto it = std::find_if(_active.begin(), _active.end(),
                                     [&](const auto& effect) { return effect.id == id && effect.remaining; });
        if (it != _active.end()) {
            *it->remaining += duration;
            it->duration = *it->remaining;
            it->stacks = std::min(it->stacks + 1, std::max(maxStacks, 1));
            return;
        }
        _active.push_back({id, duration, duration});
    }
    // Permanent effect: each addition is a separate copy and never expires.
    void AddPersistent(Id id) { _active.push_back({id, 0.0f, std::nullopt}); }

    // Refills active timed effects to their current full bar duration; stacks and permanent effects are unchanged.
    // When supplied, 'except' skips that Id. Does not restore expired effects or add duration.
    void RefreshTimed(std::optional<Id> except = std::nullopt) {
        for (auto& effect : _active)
            if (effect.remaining && (!except || effect.id != *except)) *effect.remaining = effect.duration;
    }

    // Advances time. Returns true if an effect expired.
    bool Update(float deltaSeconds) {
        for (auto& effect : _active)
            if (effect.remaining) *effect.remaining -= deltaSeconds;
        return std::erase_if(_active,
                             [](const auto& effect) { return effect.remaining && *effect.remaining <= 0.0f; }) > 0;
    }

    [[nodiscard]] const std::vector<ActiveEffect<Id>>& Active() const { return _active; }
    // First active timed effect in insertion order; nullopt if none.
    [[nodiscard]] std::optional<Id> FirstTimed() const {
        for (const auto& effect : _active)
            if (effect.remaining) return effect.id;
        return std::nullopt;
    }

private:
    std::vector<ActiveEffect<Id>> _active;
};
