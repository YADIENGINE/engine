#pragma once
#include <algorithm>
#include <cmath>
#include "Utilities/Math/Noise.h"
#include "Utilities/Math/NoiseField.h"

// How a lamp's strength moves: a smooth wander, and for a failing lamp stretches of fast hard cuts.
struct FlickerStyle {
    float depth = 0.25f;        // How far the wander dips below full, 0..1
    float speed = 4.0f;         // Changes of the wander per second
    float stutterShare = 0.0f;  // Share of the stretches that stutter, 0..1; 0: never
    float stutterEvery = 2.0f;  // Seconds per stretch
    float stutterRate = 14.0f;  // Chances per second to cut out, while stuttering
    float stutterLevel = 0.05f; // Strength while cut out, 0..1

    [[nodiscard]] static constexpr FlickerStyle Candle() { return {.depth = 0.3f, .speed = 5.0f}; }
    [[nodiscard]] static constexpr FlickerStyle DeadFluorescent() {
        return {.depth = 0.04f, .speed = 9.0f, .stutterShare = 0.35f, .stutterEvery = 1.5f};
    }
};

// A strength in 0..1 from the time alone, for LightSource::intensity or anything else that shines. It holds no state:
// the same time gives the same value, so it can be described every frame, a paused view stays still and a seek lands on
// the right look. 'seconds' is usually Painter::Time(), 'seed' keeps lamps apart, and 'flashes' is
// Game::Effects().flashes. That one is required because light sensitivity must not be forgettable: without flashes
// nothing cuts out and the wander slows down, so a candle sways and a failing lamp burns steadily.
[[nodiscard]] inline float Flicker(double seconds, const FlickerStyle& style, unsigned seed, bool flashes) {
    constexpr float GentleSpeed = 1.0f;     // Fastest wander without flashes
    constexpr float FineShare = 0.35f;      // Share of the faster second wave
    constexpr float FineSpeed = 2.7f;       // Its speed against the first
    constexpr double WrapAfter = 65536.0;   // Where a float still resolves a change; the jump is hours away
    constexpr float CutShare = 0.55f;       // Chances that cut out, while stuttering
    constexpr unsigned StretchSeed = 7919U; // Keep the two stutter patterns apart
    constexpr unsigned CutSeed = 104729U;
    if (!std::isfinite(seconds)) return 1.0f;
    const float speed = flashes ? std::max(style.speed, 0.0f) : std::clamp(style.speed, 0.0f, GentleSpeed);
    const auto at = static_cast<float>(std::fmod(seconds * static_cast<double>(speed), WrapAfter));
    const float wave = (1.0f - FineShare) * Noise(NoiseSettings{.seed = seed}, at) +
                       FineShare * Noise(NoiseSettings{.seed = seed + 1U}, at * FineSpeed);
    const float wander = 1.0f - std::clamp(style.depth, 0.0f, 1.0f) * (wave * 0.5f + 0.5f);
    if (!flashes || !(style.stutterShare > 0.0f) || !(style.stutterEvery > 0.0f) || !(style.stutterRate > 0.0f))
        return wander;
    const auto stretch = static_cast<unsigned>(static_cast<long long>(std::floor(seconds / style.stutterEvery)));
    if (Random01(stretch * StretchSeed + seed) >= style.stutterShare) return wander;
    const auto chance = static_cast<unsigned>(static_cast<long long>(std::floor(seconds * style.stutterRate)));
    return Random01(chance * CutSeed + seed) < CutShare ? std::clamp(style.stutterLevel, 0.0f, 1.0f) * wander : wander;
}
