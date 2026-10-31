#pragma once
#include <cmath>

// Animation curves and time helpers. Curves take t in 0..1 and return 0 at 0 and 1 at 1.
// Header-only.
constexpr float Clamp01(float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }

// Starts slowly, finishes accelerating.
constexpr float EaseInCubic(float t) {
    const float c = Clamp01(t);
    return c * c * c;
}

// Starts fast, stops smoothly.
constexpr float EaseOutCubic(float t) {
    const float u = 1.0f - Clamp01(t);
    return 1.0f - u * u * u;
}

// Starts slowly, accelerates in the middle, finishes slowly.
constexpr float EaseInOutCubic(float t) {
    const float c = Clamp01(t);
    if (c < 0.5f) {
        return 4.0f * c * c * c;
    }
    const float u = -2.0f * c + 2.0f;
    return 1.0f - u * u * u / 2.0f;
}

// Slightly overshoots the target and returns (pop-in).
constexpr float EaseOutBack(float t) {
    constexpr float overshoot = 1.70158f;
    const float u = Clamp01(t) - 1.0f;
    return 1.0f + (overshoot + 1.0f) * u * u * u + overshoot * u * u;
}

// Timeline helpers: 't' (age) relative to moments.
// Progress of 't' through 'from'..'to' (clamped to 0..1).
constexpr float Progress01(float t, float from, float to) { return Clamp01((t - from) / (to - from)); }
// Time since 'moment'; negative before it.
constexpr float Since(float t, float moment) { return t - moment; }
// Pulse peaking at 'at' and fading over 'seconds' (EaseOutCubic); 0 before it.
constexpr float EaseOutPulse(float t, float at, float seconds) {
    return t < at ? 0.0f : 1.0f - EaseOutCubic(Clamp01((t - at) / seconds));
}
// Staggered items: item 'index' starts at 'delay + index × step' and rises from 0 to 1 over 'seconds'.
constexpr float Staggered(float t, float delay, float index, float step, float seconds) {
    return Clamp01((t - delay - index * step) / seconds);
}
// Whether advancing from 'before' to 'after' crossed 'moment' (inclusive, once).
constexpr bool Crossed(float before, float after, float moment) { return before < moment && after >= moment; }

// Decreases 'value' by 'amount', stopping at 0. For fading counters: x = CountDown(x, dt / Seconds).
constexpr float CountDown(float value, float amount) { return value > amount ? value - amount : 0.0f; }

// Blinks every 'period' during the last 'threshold' seconds of a duration; true in the dim half.
inline bool BlinkOff(float remaining, float threshold, float period) {
    return remaining <= threshold && std::fmod(remaining, period) < period / 2.0f;
}
