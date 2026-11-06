#include "Graphics/Backgrounds/Nebula.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "Graphics/Backgrounds/StarField.h"
#include "Graphics/Paint/Painter.h"
#include "Utilities/Random/NoiseField.h"
#include "Utilities/Math/Scalar.h"

namespace {
struct NebulaColors {
    Color4 space;
    Color4 shade;
    Color4 gas;
    Color4 bright;
    Color4 accent;
};

// In NebulaKind's order.
constexpr std::array<NebulaColors, 3> Kinds{
    NebulaColors{Rgb(0x070418), Rgb(0x2a0f58), Rgb(0xa02a9a), Rgb(0xffb0c8), Rgb(0x2a4ad0)},
    NebulaColors{Rgb(0x03101c), Rgb(0x063a58), Rgb(0x10a0a0), Rgb(0xc8ffe0), Rgb(0x3a3ac8)},
    NebulaColors{Rgb(0x120406), Rgb(0x4a0e1a), Rgb(0xc8401e), Rgb(0xffb878), Rgb(0x8a1a70)},
};

constexpr int MaxRows = 64; // Mesh cells along the short side; the gas has no detail finer than this needs
constexpr int MinRows = 20;
constexpr float CellPixels = 9.0f;
constexpr float Frequency = 1.7f; // Noise cells per short side, at the largest octave
constexpr float DriftSpeed = 0.012f;
constexpr float WarpAmount = 0.55f; // How far the gas lookup is pushed, in noise cells

[[nodiscard]] float Unit(float noise) { return noise * 0.5f + 0.5f; }

[[nodiscard]] float SmoothRange(float from, float to, float value) {
    const float t = std::clamp((value - from) / (to - from), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

[[nodiscard]] Color4 GasColor(float x, float y, const NebulaColors& colors, unsigned seed, float shift) {
    // Warping the lookup by another noise turns round blobs into wisps; the warp drifts, the gas stays.
    const Float2 at{x, y};
    const Float2 drift{shift, -shift};
    const Float2 warped = Warp(NoiseSettings{.seed = seed + 17U}, WarpAmount, at + drift) - drift;
    const float gas = Unit(Noise(NoiseSettings{.fractal = NoiseFractal::FBm, .octaves = 4, .seed = seed}, warped));
    const float mask = Unit(Noise(NoiseSettings{.frequency = 0.55f, .seed = seed + 91U}, at));
    const float swirl = Unit((warped.y - y) / WarpAmount);
    const float density = SmoothRange(0.38f, 0.78f, gas) * SmoothRange(0.42f, 0.72f, mask);

    Color4 color = Mix(colors.space, colors.shade, SmoothRange(0.0f, 0.3f, density));
    color = Mix(color, colors.accent, SmoothRange(0.35f, 0.75f, swirl) * SmoothRange(0.05f, 0.35f, density) * 0.7f);
    color = Mix(color, colors.gas, SmoothRange(0.18f, 0.68f, density) * 0.9f);
    // Bright knots only: gas that reached full brightness everywhere would read as flat plateaus.
    return Mix(color, colors.bright, SmoothRange(0.62f, 1.05f, density) * 0.7f);
}

void DrawGas(Painter& paint, const FloatRect& box, const NebulaColors& colors, float shift, unsigned seed) {
    const float unit = std::min(box.width, box.height);
    const float pixels = unit * paint.PixelScale();
    const int shortCells = std::clamp(static_cast<int>(pixels / CellPixels), MinRows, MaxRows);
    const float cell = unit / static_cast<float>(shortCells);
    const int columns = std::max(1, static_cast<int>(std::ceil(box.width / cell)));
    const int rows = std::max(1, static_cast<int>(std::ceil(box.height / cell)));

    std::vector<Float2> points;
    std::vector<Color4> shades;
    std::vector<std::uint32_t> indices;
    const auto vertexCount = static_cast<std::size_t>((columns + 1) * (rows + 1));
    points.reserve(vertexCount);
    shades.reserve(vertexCount);
    indices.reserve(static_cast<std::size_t>(columns * rows) * 6);
    for (int row = 0; row <= rows; ++row) {
        const float y = std::min(static_cast<float>(row) * cell, box.height);
        for (int column = 0; column <= columns; ++column) {
            const float x = std::min(static_cast<float>(column) * cell, box.width);
            points.push_back({box.x + x, box.y + y});
            shades.push_back(GasColor(x / unit * Frequency, y / unit * Frequency, colors, seed, shift));
        }
    }
    const auto stride = static_cast<std::uint32_t>(columns + 1);
    for (std::uint32_t row = 0; row < static_cast<std::uint32_t>(rows); ++row)
        for (std::uint32_t column = 0; column < static_cast<std::uint32_t>(columns); ++column) {
            const std::uint32_t a = row * stride + column;
            const std::uint32_t b = a + 1;
            const std::uint32_t c = a + stride;
            const std::uint32_t d = c + 1;
            // Diagonals alternate; running one way, they show as a grain in steep colour changes.
            if ((row + column) % 2 == 0)
                for (const std::uint32_t corner : {a, b, c, b, d, c}) indices.push_back(corner);
            else
                for (const std::uint32_t corner : {a, b, d, a, d, c}) indices.push_back(corner);
        }
    paint.Mesh(points, shades, indices);
}
} // namespace

void DrawNebula(Painter& paint, const FloatRect& box, NebulaKind kind, float time, const NebulaLook& look) {
    if (box.Empty()) return;
    const auto clip = paint.Clip(box);
    const NebulaColors& own = Kinds[static_cast<std::size_t>(kind)];
    const NebulaColors colors{look.space.value_or(own.space), look.shade.value_or(own.shade),
                              look.gas.value_or(own.gas), look.bright.value_or(own.bright),
                              look.accent.value_or(own.accent)};
    DrawGas(paint, box, colors, time * DriftSpeed * look.speed, look.seed * 977U + static_cast<unsigned>(kind) * 31U);
    if (look.stars > 0.0f)
        DrawStarField(paint, box, StarFieldKind::Still, time,
                      {.background = false, .density = look.stars, .seed = look.seed + 5U});
}
