#pragma once
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

// Smooth noise in 0..1 over a plane, with a different field for each seed. One new value per unit.
[[nodiscard]] inline float ValueNoise(float x, float y, unsigned seed) {
    const float cellX = std::floor(x);
    const float cellY = std::floor(y);
    const float tx = x - cellX;
    const float ty = y - cellY;
    const float sx = tx * tx * (3.0f - 2.0f * tx);
    const float sy = ty * ty * (3.0f - 2.0f * ty);
    const auto ix = static_cast<unsigned>(static_cast<int>(cellX));
    const auto iy = static_cast<unsigned>(static_cast<int>(cellY));
    const auto corner = [seed](unsigned px, unsigned py) { return Random01(px * 73856093U ^ py * 19349663U ^ seed); };
    const float top = Mix(corner(ix, iy), corner(ix + 1U, iy), sx);
    return Mix(top, Mix(corner(ix, iy + 1U), corner(ix + 1U, iy + 1U), sx), sy);
}

// Smooth noise in -1..1 along 'x', with a different path for each seed (a shaking camera). One new value per unit.
inline float SmoothNoise(float x, unsigned seed) {
    const float cell = std::floor(x);
    const float t = x - cell;
    const auto at = static_cast<unsigned>(static_cast<long long>(cell));
    const auto value = [seed](unsigned point) { return Random01(point * 2654435761U + seed) * 2.0f - 1.0f; };
    return Mix(value(at), value(at + 1U), t * t * (3.0f - 2.0f * t));
}
