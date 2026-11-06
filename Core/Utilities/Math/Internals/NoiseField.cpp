#include "Utilities/Math/NoiseField.h"
#include <FastNoiseLite.h>
#include <algorithm>
#include <cmath>

namespace {
// FastNoiseLite's warp pushes a fraction of its amplitude; these turn 'amplitude' into the largest push of one layer.
// Measured over 200,000 points: OpenSimplex2 reaches 0.22 of it, the grid warp 0.57.
constexpr float SmoothWarpScale = 4.5f;
constexpr float GridWarpScale = 1.75f;

FastNoiseLite::NoiseType ToType(NoiseKind kind) {
    switch (kind) {
    case NoiseKind::Simplex      : return FastNoiseLite::NoiseType_OpenSimplex2;
    case NoiseKind::SimplexSmooth: return FastNoiseLite::NoiseType_OpenSimplex2S;
    case NoiseKind::Cellular     : return FastNoiseLite::NoiseType_Cellular;
    case NoiseKind::Perlin       : return FastNoiseLite::NoiseType_Perlin;
    case NoiseKind::Value        : return FastNoiseLite::NoiseType_Value;
    }
    return FastNoiseLite::NoiseType_OpenSimplex2;
}

// Each kind's frequency scaled so that every kind changes as often as values set one unit apart (value noise): about
// 0.67 turning points per unit along a line. Measured over 50 lines of 200 units each.
float FrequencyScale(NoiseKind kind) {
    switch (kind) {
    case NoiseKind::Simplex      : return 0.305f;
    case NoiseKind::SimplexSmooth: return 0.355f;
    case NoiseKind::Cellular     : return 0.322f;
    case NoiseKind::Perlin       : return 0.479f;
    case NoiseKind::Value        : return 1.0f;
    }
    return 1.0f;
}

FastNoiseLite::FractalType ToFractal(NoiseFractal fractal) {
    switch (fractal) {
    case NoiseFractal::None    : return FastNoiseLite::FractalType_None;
    case NoiseFractal::FBm     : return FastNoiseLite::FractalType_FBm;
    case NoiseFractal::Ridged  : return FastNoiseLite::FractalType_Ridged;
    case NoiseFractal::PingPong: return FastNoiseLite::FractalType_PingPong;
    }
    return FastNoiseLite::FractalType_None;
}

FastNoiseLite Generator(const NoiseSettings& settings) {
    FastNoiseLite noise(static_cast<int>(settings.seed));
    noise.SetFrequency((std::isfinite(settings.frequency) ? settings.frequency : 1.0f) * FrequencyScale(settings.kind));
    noise.SetNoiseType(ToType(settings.kind));
    noise.SetFractalOctaves(std::clamp(settings.octaves, 1, 16));
    noise.SetFractalLacunarity(settings.lacunarity);
    noise.SetFractalGain(settings.gain);
    return noise;
}

float Finite(float value) { return std::isfinite(value) ? value : 0.0f; }
} // namespace

float Noise(const NoiseSettings& settings, Float2 point) {
    FastNoiseLite noise = Generator(settings);
    noise.SetFractalType(ToFractal(settings.fractal));
    return std::clamp(noise.GetNoise(Finite(point.x), Finite(point.y)), -1.0f, 1.0f);
}

float Noise(const NoiseSettings& settings, float x) { return Noise(settings, Float2{x, 0.0f}); }

Float2 Warp(const NoiseSettings& settings, float amplitude, Float2 point) {
    FastNoiseLite noise = Generator(settings);
    const bool smooth = settings.kind == NoiseKind::Simplex || settings.kind == NoiseKind::SimplexSmooth;
    noise.SetDomainWarpType(smooth ? FastNoiseLite::DomainWarpType_OpenSimplex2
                                   : FastNoiseLite::DomainWarpType_BasicGrid);
    noise.SetDomainWarpAmp(Finite(amplitude) * (smooth ? SmoothWarpScale : GridWarpScale));
    noise.SetFractalType(settings.fractal == NoiseFractal::None ? FastNoiseLite::FractalType_None
                                                                : FastNoiseLite::FractalType_DomainWarpProgressive);
    float x = Finite(point.x);
    float y = Finite(point.y);
    noise.DomainWarp(x, y);
    return {x, y};
}
