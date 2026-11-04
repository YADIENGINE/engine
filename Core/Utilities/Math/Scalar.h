#pragma once
#include <cmath>

// Shared scalar helpers for views (no raylib). Header-only.
inline constexpr float Pi = 3.14159265f;

// Degrees to radians; positive angles turn clockwise in canvas coordinates.
[[nodiscard]] constexpr float Radians(float degrees) { return degrees * (Pi / 180.0f); }

// Linear interpolation from a to b by 't' (not clamped). Named Mix to avoid raymath's Lerp.
constexpr float Mix(float a, float b, float t) { return a + (b - a) * t; }

// Fractional part (0..1).
inline float Fract(float value) { return value - std::floor(value); }
