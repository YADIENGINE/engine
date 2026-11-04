#include "Graphics/Backgrounds/Sea.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
#include "Graphics/Paint/Painter.h"
#include "Utilities/Math/Noise.h"
#include "Utilities/Math/Scalar.h"

namespace {
struct SeaColors {
    Color4 far;
    Color4 middle; // Far to near through this keeps the colour clean; two complementary ends would mix to mud
    Color4 near;
    Color4 crest;
    Color4 glint;
    Float2 sunAt;  // In the part of the box above the horizon
    float sunSize; // Of that part's short side
};

// In SkyKind's order.
constexpr std::array<SeaColors, 3> Kinds{
    SeaColors{Rgb(0x7ccde6), Rgb(0x2b92c8), Rgb(0x0b4a84), Rgb(0xe4f6ff, 120), Rgb(0xfffef0), {0.72f, 0.32f}, 0.08f},
    SeaColors{Rgb(0xffa070), Rgb(0xc44f84), Rgb(0x2f1c64), Rgb(0xffc09a, 100), Rgb(0xffe08a), {0.5f, 0.86f}, 0.17f},
    SeaColors{Rgb(0x2c4378), Rgb(0x142452), Rgb(0x050b24), Rgb(0x8ea6dc, 70), Rgb(0xdfe6ff), {0.7f, 0.34f}, 0.09f},
};

constexpr int WaveRows = 13;
constexpr int MaxPointsPerRow = 110;
constexpr float DepthCurve = 1.75f; // Rows crowd towards the horizon
constexpr int GlintsPerRow = 7;

[[nodiscard]] float Pick(unsigned seed, unsigned row, unsigned channel) {
    return Random01(seed * 6700417U + row * 2147483629U + channel * 524287U);
}

void DrawGlints(Painter& paint, const FloatRect& water, float bodyX, float bodyRadius, Color4 color, float time,
                float speed, unsigned seed) {
    const float unit = std::min(water.width, water.height * 2.0f);
    const float pixel = 1.0f / std::max(paint.PixelScale(), 0.001f);
    const int count = WaveRows * GlintsPerRow;
    for (int i = 0; i < count; ++i) {
        const auto glint = static_cast<unsigned>(i);
        const float z = std::pow(Pick(seed, glint, 0), 1.35f);
        const float y = water.y + water.height * z;
        const float spread = bodyRadius * (0.7f + 2.6f * z);
        const float sway = std::sin(time * speed * (0.8f + Pick(seed, glint, 1)) + Pick(seed, glint, 2) * 6.28f);
        const float x = bodyX + (Pick(seed, glint, 3) * 2.0f - 1.0f) * spread + sway * unit * 0.006f;
        const float blink = 0.5f + 0.5f * std::sin(time * speed * (1.4f + Pick(seed, glint, 4) * 2.0f) +
                                                   Pick(seed, glint, 5) * 6.28f);
        const float half = spread * (0.12f + 0.34f * Pick(seed, glint, 6)) * (0.55f + 0.45f * blink);
        const float thick = std::max(unit * (0.0014f + 0.0048f * z), pixel * 0.5f);
        paint.Ellipse({x, y}, {half, thick}, Fill(ScaleAlpha(color, 0.35f + 0.6f * blink)));
    }
}
} // namespace

void DrawSea(Painter& paint, const FloatRect& box, SkyKind kind, float time, const SeaLook& look) {
    if (box.Empty()) return;
    const auto clip = paint.Clip(box);
    const SeaColors& own = Kinds[static_cast<std::size_t>(kind)];
    const float horizon = std::clamp(look.horizon, 0.05f, 0.95f);
    const FloatRect air{box.x, box.y, box.width, box.height * horizon};
    const FloatRect water{box.x, air.Bottom(), box.width, box.height - air.height};
    const float pixel = 1.0f / std::max(paint.PixelScale(), 0.001f);
    const float bodyRadius = own.sunSize * std::min(air.width, air.height);
    // The sky runs a pixel under the water, or what lies behind the box could show through along the horizon.
    const FloatRect behind{air.x, air.y, air.width, air.height + pixel * 2.0f};
    if (look.sky) DrawSky(paint, behind, kind, time, {.bodyAt = own.sunAt, .bodySize = own.sunSize, .seed = look.seed});

    const Color4 farColor = look.far.value_or(own.far);
    const Color4 middleColor = look.middle.value_or(own.middle);
    const Color4 nearColor = look.near.value_or(own.near);
    const Color4 crestColor = look.crest.value_or(own.crest);

    const std::array bands{farColor, middleColor, nearColor};
    const auto shadeAt = [&](float z) {
        return z < 0.5f ? Mix(farColor, middleColor, z * 2.0f) : Mix(middleColor, nearColor, z * 2.0f - 1.0f);
    };
    DrawGradientBands(paint, water, bands);

    const float clock = time * look.speed;
    std::vector<Float2> shape;
    std::vector<Float2> crest;
    for (int row = 0; row < WaveRows; ++row) {
        const auto rowIndex = static_cast<unsigned>(row);
        const float z = static_cast<float>(row + 1) / static_cast<float>(WaveRows);
        const float depth = std::pow(z, DepthCurve);
        const float y = water.y + water.height * depth * 0.96f;
        const float wavelength = water.height * (0.1f + 0.85f * depth);
        const float amplitude = water.height * (0.004f + 0.05f * depth);
        const float phase = Pick(look.seed, rowIndex, 0) * Pi * 2.0f;
        const float direction = row % 2 == 0 ? 1.0f : -1.0f;
        const int points = std::clamp(static_cast<int>(box.width / wavelength * 12.0f), 12, MaxPointsPerRow);

        crest.clear();
        for (int i = 0; i <= points; ++i) {
            const float x = box.x + box.width * static_cast<float>(i) / static_cast<float>(points);
            const float along = (x - box.x) / wavelength * Pi * 2.0f;
            const float wave = std::sin(along + clock * direction * 0.9f + phase) * 0.62f +
                               std::sin(along * 1.73f - clock * direction * 0.6f + phase * 2.0f) * 0.38f;
            crest.push_back({x, y - std::abs(wave) * amplitude * 1.1f + amplitude * 0.5f});
        }
        shape = crest;
        shape.push_back({box.Right(), box.Bottom()});
        shape.push_back({box.x, box.Bottom()});
        // Every other row is a shade lighter, so that rows read as bands of water and not as lines on a flat sheet.
        const Color4 color = Mix(shadeAt(depth), WithAlpha(crestColor, 1.0f), row % 2 == 0 ? 0.0f : 0.09f);
        paint.Polygon(shape, Fill(color));
        paint.Polyline(crest, {.width = std::max(water.height * (0.0014f + 0.0042f * depth), pixel * 0.6f),
                               .color = ScaleAlpha(crestColor, 0.2f + 0.6f * z),
                               .cap = StrokeCap::Round,
                               .join = StrokeJoin::Round});
    }

    const float bodyX = box.x + own.sunAt.x * box.width;
    DrawGlints(paint, water, bodyX, bodyRadius, look.glint.value_or(own.glint), time, look.speed, look.seed + 3U);
}
