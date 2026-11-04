#pragma once
#include <cmath>
#include "Utilities/Math/Scalar.h"

// Animation curves and time helpers. Curves take t in 0..1 and return 0 at 0 and 1 at 1; t outside that is clamped.
// Header-only. Simulations may use all of them: the Sine, Expo and Elastic curves call the standard library's math,
// which is covered by the same-build guarantee (Docs/Animation.md).
constexpr float Clamp01(float t) { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }

// Constant speed.
constexpr float EaseLinear(float t) { return Clamp01(t); }

// Gentle versions of the cubic curves below.
constexpr float EaseInQuad(float t) {
    const float c = Clamp01(t);
    return c * c;
}
constexpr float EaseOutQuad(float t) {
    const float u = 1.0f - Clamp01(t);
    return 1.0f - u * u;
}
constexpr float EaseInOutQuad(float t) {
    const float c = Clamp01(t);
    if (c < 0.5f) {
        return 2.0f * c * c;
    }
    const float u = -2.0f * c + 2.0f;
    return 1.0f - u * u / 2.0f;
}

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

// Sharper versions of the cubic curves.
constexpr float EaseInQuart(float t) {
    const float c = Clamp01(t);
    return c * c * c * c;
}
constexpr float EaseOutQuart(float t) {
    const float u = 1.0f - Clamp01(t);
    return 1.0f - u * u * u * u;
}
constexpr float EaseInOutQuart(float t) {
    const float c = Clamp01(t);
    if (c < 0.5f) {
        return 8.0f * c * c * c * c;
    }
    const float u = -2.0f * c + 2.0f;
    return 1.0f - u * u * u * u / 2.0f;
}

// Softest of the smooth curves.
inline float EaseInSine(float t) { return 1.0f - std::cos(Clamp01(t) * Pi / 2.0f); }
inline float EaseOutSine(float t) { return std::sin(Clamp01(t) * Pi / 2.0f); }
inline float EaseInOutSine(float t) { return (1.0f - std::cos(Clamp01(t) * Pi)) / 2.0f; }

// Nearly still, then very fast; and the reverse.
inline float EaseInExpo(float t) {
    const float c = Clamp01(t);
    return c <= 0.0f ? 0.0f : std::pow(2.0f, 10.0f * c - 10.0f);
}
inline float EaseOutExpo(float t) {
    const float c = Clamp01(t);
    return c >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * c);
}

// Pulls back before leaving (wind-up).
constexpr float EaseInBack(float t) {
    constexpr float overshoot = 1.70158f;
    const float c = Clamp01(t);
    return c >= 1.0f ? 1.0f : c * c * ((overshoot + 1.0f) * c - overshoot);
}
// Slightly overshoots the target and returns (pop-in).
constexpr float EaseOutBack(float t) {
    constexpr float overshoot = 1.70158f;
    const float u = Clamp01(t) - 1.0f;
    return 1.0f + (overshoot + 1.0f) * u * u * u + overshoot * u * u;
}
// Pulls back, crosses over, overshoots and returns.
constexpr float EaseInOutBack(float t) {
    constexpr float overshoot = 1.70158f * 1.525f;
    const float c = Clamp01(t);
    if (c >= 1.0f) {
        return 1.0f;
    }
    if (c < 0.5f) {
        const float u = 2.0f * c;
        return u * u * ((overshoot + 1.0f) * u - overshoot) / 2.0f;
    }
    const float u = 2.0f * c - 2.0f;
    return (u * u * ((overshoot + 1.0f) * u + overshoot) + 2.0f) / 2.0f;
}

// Shivers around the target and settles there (a spring hit); the reverse for a shiver that builds up.
inline float EaseOutElastic(float t) {
    const float c = Clamp01(t);
    if (c <= 0.0f || c >= 1.0f) {
        return c;
    }
    return std::pow(2.0f, -10.0f * c) * std::sin((10.0f * c - 0.75f) * (2.0f * Pi / 3.0f)) + 1.0f;
}
inline float EaseInElastic(float t) {
    const float c = Clamp01(t);
    if (c <= 0.0f || c >= 1.0f) {
        return c;
    }
    return -std::pow(2.0f, 10.0f * c - 10.0f) * std::sin((10.0f * c - 10.75f) * (2.0f * Pi / 3.0f));
}

// Falls on the target and bounces a few times (a landing); the reverse for bounces that grow into the move.
constexpr float EaseOutBounce(float t) {
    constexpr float strength = 7.5625f;
    constexpr float width = 2.75f;
    const float c = Clamp01(t);
    if (c >= 1.0f) {
        return 1.0f;
    }
    if (c < 1.0f / width) {
        return strength * c * c;
    }
    if (c < 2.0f / width) {
        const float u = c - 1.5f / width;
        return strength * u * u + 0.75f;
    }
    if (c < 2.5f / width) {
        const float u = c - 2.25f / width;
        return strength * u * u + 0.9375f;
    }
    const float u = c - 2.625f / width;
    return strength * u * u + 0.984375f;
}
constexpr float EaseInBounce(float t) { return 1.0f - EaseOutBounce(1.0f - Clamp01(t)); }

// Time helpers for one or two moments: 't' (age) relative to them. For a named sequence use Animation/Timeline.h.
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

// What remains of a value decaying at 'rate' per second after 'seconds'. Split a time in two and the result is the
// same, so unlike x -= x * rate * dt it does not depend on the frame rate.
inline float DecayFactor(float rate, float seconds) { return std::exp(-rate * seconds); }

// Blinks every 'period' during the last 'threshold' seconds of a duration; true in the dim half.
inline bool BlinkOff(float remaining, float threshold, float period) {
    return remaining <= threshold && std::fmod(remaining, period) < period / 2.0f;
}
