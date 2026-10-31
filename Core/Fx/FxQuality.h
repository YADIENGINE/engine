#pragma once
#include <algorithm>

// How strongly an effect plays: Full; Reduced (milder); Off.
enum class EffectLevel { Full, Reduced, Off };

// The player's choice for simulated effects (older machines, light sensitivity). The Fx classes apply it themselves
// (their SetQuality); callers do not branch on it. Rendering cost (glows, bloom, background animation) is
// RenderQuality, applied by the Painter. Game::Effects() holds the current choice.
struct FxQuality {
    static constexpr float ReducedShake = 0.4f;

    int particlePercent = 100;             // Share of emitted particles, 0..100
    EffectLevel shake = EffectLevel::Full; // Camera shake, punch and kick
    bool motion = true;                    // Speed effects: camera lead and speed lines (games: their own trails)
    bool flashes = true;                   // Screen flashes and glitches

    // Shake amplitude multiplier (Full 1, Reduced milder, Off 0).
    [[nodiscard]] constexpr float ShakeScale() const {
        switch (shake) {
        case EffectLevel::Full   : return 1.0f;
        case EffectLevel::Reduced: return ReducedShake;
        case EffectLevel::Off    : return 0.0f;
        }
        return 1.0f;
    }
    // 'count' scaled by particlePercent; a non-zero request makes at least one particle.
    [[nodiscard]] constexpr int ParticleCount(int count) const {
        const int percent = std::clamp(particlePercent, 0, 100);
        if (count <= 0 || percent == 0) return 0;
        return std::max(1, count * percent / 100);
    }
    friend constexpr bool operator==(const FxQuality&, const FxQuality&) = default;
};
