#pragma once
#include <cmath>
#include <cstddef>

// Shared numeric helpers for views (no raylib). Header-only.
inline constexpr float Pi = 3.14159265f;

// Degrees to radians; positive angles turn clockwise in canvas coordinates.
[[nodiscard]] constexpr float Radians(float degrees) { return degrees * (Pi / 180.0f); }

// Linear interpolation from a to b by 't' (not clamped). Named Mix to avoid raymath's Lerp.
constexpr float Mix(float a, float b, float t) { return a + (b - a) * t; }

// Fractional part (0..1).
inline float Fract(float value) { return value - std::floor(value); }

// Stable pseudorandom number from a seed: the same seed gives the same result each frame (for steady decorations).
constexpr unsigned Hash(unsigned value) {
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}
// Stable number in 0..1 (a thousand steps) from a seed.
constexpr float Random01(unsigned seed) { return static_cast<float>(Hash(seed) % 1000U) / 1000.0f; }
// Stable noise in 0..1 from two integers (sine-based; for cell / particle decorations). Distributed differently from
// Hash.
inline float NoiseHash(int a, int b) {
    return Fract(std::sin(static_cast<float>(a) * 12.9898f + static_cast<float>(b) * 78.233f) * 43758.547f);
}
// Wraps 'value' into 0..count-1 (negative values wrap from the end); count > 0.
constexpr int WrapIndex(int value, int count) { return (value % count + count) % count; }
// Clamps 'value' to 0..count-1 and converts it to an array index (for level tables); count > 0.
constexpr std::size_t ClampIndex(int value, int count) {
    return static_cast<std::size_t>(value < 0 ? 0 : (value > count - 1 ? count - 1 : value));
}
