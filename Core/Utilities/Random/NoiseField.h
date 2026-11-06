#pragma once
#include "Utilities/Geometry/Float2.h"

// Smooth noise: a field of values in -1..1 that changes gradually over a plane, for clouds, gas, terrain outlines,
// wobbles and shakes. Plain functions of their settings and point: the same settings and point give the same value
// in every frame, so drawing that uses it needs no state. Built with precise floats, so simulations may use it too
// (the same build gives the same values). Stable hashes for decorations stay in Noise.h.
//     const NoiseSettings clouds{.frequency = 0.01f, .fractal = NoiseFractal::FBm, .seed = 7};
//     const float cover = Noise(clouds, point) * 0.5f + 0.5f; // 0..1

// Simplex: smooth, without the grid lines value noise shows. SimplexSmooth: rounder, a little slower. Cellular:
// distance to the nearest of scattered points (cells, stones, caustics). Perlin: classic gradient noise. Value: blocky
// interpolated values on a grid.
enum class NoiseKind { Simplex, SimplexSmooth, Cellular, Perlin, Value };
// Layers of finer noise added on top ('octaves'): FBm (soft detail), Ridged (sharp crests), PingPong (bands).
enum class NoiseFractal { None, FBm, Ridged, PingPong };

struct NoiseSettings {
    NoiseKind kind = NoiseKind::Simplex;
    float frequency = 1.0f; // Changes per unit: at 1 every kind changes about as often as values set a unit apart
    NoiseFractal fractal = NoiseFractal::None;
    int octaves = 3;         // Layers when 'fractal' is set
    float lacunarity = 2.0f; // Each layer this many times finer
    float gain = 0.5f;       // ... and this many times weaker
    unsigned seed = 0;       // A different field for each seed
};

// The field's value at 'point', in -1..1.
[[nodiscard]] float Noise(const NoiseSettings& settings, Float2 point);
// A straight line through the field, in -1..1: a value that wanders smoothly along 'x' (camera shake, flicker, an
// outline). Unlike value noise it never slows to a stop at whole numbers.
[[nodiscard]] float Noise(const NoiseSettings& settings, float x);
// 'point' pushed by up to about 'amplitude' along a smooth field (domain warp): sampling another noise at the result
// turns round blobs into wisps and swirls. 'kind' Simplex or SimplexSmooth warps smoothly, the others along a grid;
// a 'fractal' adds finer pushes on top, which reach up to about half as far again.
[[nodiscard]] Float2 Warp(const NoiseSettings& settings, float amplitude, Float2 point);
