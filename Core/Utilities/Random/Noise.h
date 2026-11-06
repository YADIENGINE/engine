#pragma once
// Stable hashes for decorations. Smooth noise (clouds, shakes, outlines) is in NoiseField.h.
#include <cmath>
#include "Utilities/Math/Scalar.h"

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
