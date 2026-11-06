#include "Graphics/Backgrounds/SynthwaveHorizon.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
#include "Graphics/Backgrounds/Sky.h"
#include "Graphics/Backgrounds/StarField.h"
#include "Graphics/Paint/Painter.h"
#include "Utilities/Math/NoiseField.h"
#include "Utilities/Math/Scalar.h"

namespace {
struct SynthwaveColors {
    Color4 skyTop;
    Color4 skyMiddle;
    Color4 skyHorizon;
    Color4 sunTop;
    Color4 sunBottom;
    Color4 ridge;
    Color4 rim;
    Color4 ground;
    Color4 grid;
};

// In SynthwaveKind's order.
constexpr std::array<SynthwaveColors, 3> Kinds{
    SynthwaveColors{Rgb(0x0b0528), Rgb(0x34105e), Rgb(0xb4247e), Rgb(0xffe25a), Rgb(0xff2a8a), Rgb(0x180836),
                    Rgb(0xff5ac0), Rgb(0x10042a), Rgb(0xff3ac8)},
    SynthwaveColors{Rgb(0x020a26), Rgb(0x0a2a60), Rgb(0x1a7ab0), Rgb(0xd8fffa), Rgb(0x28a8ff), Rgb(0x041230),
                    Rgb(0x5af0ff), Rgb(0x020a22), Rgb(0x30e8ff)},
    SynthwaveColors{Rgb(0x160418), Rgb(0x4a0c28), Rgb(0xc8381c), Rgb(0xfff09a), Rgb(0xff4a1c), Rgb(0x240616),
                    Rgb(0xffa040), Rgb(0x180410), Rgb(0xff8a28)},
};

constexpr float SunSize = 0.27f; // Radius, of the box's short side
constexpr int SunBands = 7;
constexpr float BandDrift = 0.16f; // Bands per second sinking through the sun

constexpr int GridRows = 17;
constexpr float GridCurve = 2.7f;      // Rows crowd towards the horizon
constexpr float GridRushSpeed = 0.55f; // Rows per second
constexpr int GridColumnsPerSide = 18;
constexpr float GridColumnSpacing = 0.46f; // Between columns at the bottom edge, of the short side

constexpr int RidgePoints = 72;

void DrawStripedSun(Painter& paint, const FloatRect& box, Float2 center, float radius, float clock, Color4 top,
                    Color4 bottom) {
    paint.Glow(center, radius * 2.4f, {.color = ScaleAlpha(bottom, 0.5f)});
    ShapeStyle disc;
    disc.gradient = Gradient::Vertical(radius * 2.0f, top, bottom);
    disc.emission = 0.35f;
    const float left = center.x - radius - 1.0f;
    const float width = radius * 2.0f + 2.0f;

    // Solid above the middle, then slices that thin out on the way down, with real gaps between them.
    const float solidEnd = center.y - radius * 0.08f;
    {
        const auto strip = paint.Clip({left, box.y, width, solidEnd - box.y});
        paint.Circle(center, radius, disc);
    }
    const float span = center.y + radius - solidEnd;
    const float pitch = span / static_cast<float>(SunBands);
    const float flow = Fract(clock * BandDrift);
    for (int band = -1; band < SunBands; ++band) {
        const float t = (static_cast<float>(band) + flow) / static_cast<float>(SunBands);
        const float y = solidEnd + span * t;
        const float gap = pitch * (0.14f + 0.5f * std::max(t, 0.0f));
        const float from = std::max(y + gap, solidEnd);
        const float to = y + pitch;
        if (to <= from) continue;
        const auto strip = paint.Clip({left, from, width, to - from});
        paint.Circle(center, radius, disc);
    }
}

void DrawRidges(Painter& paint, const FloatRect& box, float horizonY, Color4 ridge, Color4 rim, unsigned seed) {
    const float unit = std::min(box.width, box.height);
    std::vector<Float2> crest;
    std::vector<Float2> shape;
    crest.reserve(RidgePoints + 1);
    for (int i = 0; i <= RidgePoints; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(RidgePoints);
        const float x = box.x + t * box.width;
        // Low in the middle, so the sun stands clear; a mirror image reads as a designed scene.
        const float side = std::abs(t - 0.5f) * 2.0f;
        const float world = side * box.width / unit;
        const float fold = 1.0f - std::abs(Noise(NoiseSettings{.seed = seed}, world * 3.1f + 2.0f));
        const float detail = 1.0f - std::abs(Noise(NoiseSettings{.seed = seed + 19U}, world * 8.3f + 5.0f));
        const float envelope = std::clamp((side - 0.22f) * 1.5f, 0.0f, 1.0f);
        const float height = unit * (0.004f + envelope * (0.13f * fold * fold + 0.035f * detail));
        crest.push_back({x, horizonY - height});
    }
    shape = crest;
    // Well under the horizon, where the ground covers it: the shape must stay thick even where the ridge is flat.
    shape.push_back({box.Right(), horizonY + unit * 0.05f});
    shape.push_back({box.x, horizonY + unit * 0.05f});
    paint.Polygon(shape, Fill(ridge));
    paint.Polyline(crest, {.width = std::max(unit * 0.0028f, 1.0f / std::max(paint.PixelScale(), 0.001f)),
                           .color = ScaleAlpha(rim, 0.85f),
                           .join = StrokeJoin::Round});
}

void DrawGrid(Painter& paint, const FloatRect& ground, float unit, float clock, Color4 grid) {
    const auto clip = paint.Clip(ground);
    const float pixel = 1.0f / std::max(paint.PixelScale(), 0.001f);
    const Float2 vanish{ground.Center().x, ground.y};
    const float halo = unit * 0.016f;
    for (int column = -GridColumnsPerSide; column <= GridColumnsPerSide; ++column) {
        const float bottomX = vanish.x + (static_cast<float>(column) + 0.5f) * unit * GridColumnSpacing;
        // Outer columns lie almost along the horizon: with a glow of their own they would pile up into a white band.
        const float outer = std::min(std::abs(static_cast<float>(column)) / 7.0f, 1.0f);
        paint.Line(vanish, {bottomX, ground.Bottom()},
                   {.width = std::max(unit * 0.0024f, pixel),
                    .color = ScaleAlpha(grid, 0.75f - 0.45f * outer),
                    .glow = {.radius = halo * (1.0f - outer), .color = ScaleAlpha(grid, 0.3f)},
                    .emission = 0.3f});
    }
    const float flow = Fract(clock * GridRushSpeed);
    for (int row = 0; row < GridRows; ++row) {
        const float z = (static_cast<float>(row) + flow) / static_cast<float>(GridRows);
        const float y = ground.y + ground.height * std::pow(z, GridCurve);
        paint.Line({ground.x, y}, {ground.Right(), y},
                   {.width = std::max(unit * (0.0014f + 0.0028f * z), pixel),
                    .color = ScaleAlpha(grid, std::min(0.12f + z * 1.25f, 1.0f)),
                    .glow = {.radius = halo * z, .color = ScaleAlpha(grid, 0.35f * z)},
                    .emission = 0.3f});
    }
}
} // namespace

void DrawSynthwaveHorizon(Painter& paint, const FloatRect& box, SynthwaveKind kind, float time,
                          const SynthwaveLook& look) {
    if (box.Empty()) return;
    const auto clip = paint.Clip(box);
    const SynthwaveColors& own = Kinds[static_cast<std::size_t>(kind)];
    const Color4 gridColor = look.grid.value_or(own.grid);
    const Color4 skyHorizon = look.skyHorizon.value_or(own.skyHorizon);
    const float unit = std::min(box.width, box.height);
    const float horizonY = box.y + box.height * std::clamp(look.horizon, 0.1f, 0.9f);
    const float clock = time * look.speed;

    const FloatRect air{box.x, box.y, box.width, horizonY - box.y};
    const std::array bands{look.skyTop.value_or(own.skyTop), look.skyMiddle.value_or(own.skyMiddle), skyHorizon};
    DrawGradientBands(paint, air, bands);
    DrawStarField(paint, air, StarFieldKind::Still, time,
                  {.background = false, .density = 0.55f, .fade = 1.0f, .seed = look.seed + 2U});

    const float radius = unit * SunSize;
    const Float2 sunCenter{box.Center().x, horizonY - radius * 0.72f};
    {
        const auto sky = paint.Clip(air);
        DrawStripedSun(paint, air, sunCenter, radius, clock, look.sunTop.value_or(own.sunTop),
                       look.sunBottom.value_or(own.sunBottom));
    }
    DrawRidges(paint, box, horizonY, look.ridge.value_or(own.ridge), look.rim.value_or(own.rim), look.seed * 151U);

    const FloatRect ground{box.x, horizonY, box.width, box.Bottom() - horizonY};
    ShapeStyle floor;
    floor.gradient = Gradient::Vertical(ground.height, Mix(look.ground.value_or(own.ground), skyHorizon, 0.16f),
                                        look.ground.value_or(own.ground));
    paint.Rect(ground, floor);
    DrawGrid(paint, ground, unit, clock, gridColor);

    paint.Line({box.x, horizonY}, {box.Right(), horizonY},
               {.width = std::max(unit * 0.004f, 1.0f / std::max(paint.PixelScale(), 0.001f)),
                .color = Mix(gridColor, Rgb(0xffffff), 0.45f),
                .glow = {.radius = unit * 0.035f, .color = ScaleAlpha(gridColor, 0.45f)},
                .emission = 0.5f});
}
