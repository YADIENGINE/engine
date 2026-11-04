#include "Graphics/Backgrounds/Skyline.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include "Graphics/Paint/Painter.h"
#include "Utilities/Math/Noise.h"
#include "Utilities/Math/Scalar.h"

namespace {
struct SkylineColors {
    std::array<Color4, 3> buildings;
    Color4 window;
    Color4 windowAlt;
    Color4 haze;
    Color4 beacon;
    float lit; // Share of windows that show
};

// In SkyKind's order.
constexpr std::array<SkylineColors, 3> Kinds{
    SkylineColors{{Rgb(0xa8c2dc), Rgb(0x6c88a8), Rgb(0x35506e)},
                  Rgb(0xd8ecfa, 165),
                  Rgb(0xa6cbe4, 165),
                  Rgb(0xdcecf6),
                  Rgb(0xe04040),
                  0.42f},
    SkylineColors{{Rgb(0xb4628a), Rgb(0x6c3a7c), Rgb(0x2a1a48)},
                  Rgb(0xffcf7a),
                  Rgb(0xff9a6a),
                  Rgb(0xff9a68),
                  Rgb(0xff4a4a),
                  0.24f},
    SkylineColors{{Rgb(0x1e2c58), Rgb(0x121c40), Rgb(0x080d24)},
                  Rgb(0xffd98a),
                  Rgb(0x8ad8ff),
                  Rgb(0x3a4e8a),
                  Rgb(0xff4050),
                  0.36f},
};

struct BuildingRow {
    float cell;      // Width of a building's plot, of the box height
    float minHeight; // Of the box height
    float maxHeight;
    float parallax; // Share of the scroll this row follows
    float haze;     // Mist in front of the row
    bool windows;
};
constexpr std::array<BuildingRow, 3> Rows{
    BuildingRow{0.075f, 0.2f, 0.62f, 0.3f, 0.5f, false},
    BuildingRow{0.1f, 0.14f, 0.46f, 0.58f, 0.34f, true},
    BuildingRow{0.135f, 0.07f, 0.31f, 1.0f, 0.0f, true},
};

constexpr float WindowWidth = 0.0065f; // Of the box height
constexpr float MinWindowPixels = 1.6f;
constexpr float WindowHeightRatio = 1.45f;
constexpr float WindowGapRatio = 2.0f;    // Window pitch to window width
constexpr float WindowChangeRate = 0.05f; // Changes per second of a window that changes
constexpr float ChangingShare = 0.14f;    // Share of windows that change

[[nodiscard]] float Pick(unsigned seed, unsigned building, unsigned channel) {
    return Random01(seed * 433494437U + building * 2971215073U + channel * 1500450271U);
}

void DrawWindows(Painter& paint, const FloatRect& wall, float windowWidth, Color4 window, Color4 windowAlt, float lit,
                 float time, unsigned seed) {
    const float pitchX = windowWidth * WindowGapRatio;
    const float windowHeight = windowWidth * WindowHeightRatio;
    const float pitchY = windowHeight * 1.7f;
    const int columns = static_cast<int>((wall.width - windowWidth) / pitchX);
    const int rows = static_cast<int>((wall.height - windowHeight * 1.6f) / pitchY);
    if (columns <= 0 || rows <= 0) return;
    const float left = wall.x + (wall.width - (static_cast<float>(columns - 1) * pitchX + windowWidth)) * 0.5f;
    for (int row = 0; row < rows; ++row)
        for (int column = 0; column < columns; ++column) {
            const auto id = static_cast<unsigned>(row * 257 + column);
            float state = Pick(seed, id, 0);
            if (Pick(seed, id, 1) < ChangingShare) {
                const float change = std::floor(time * WindowChangeRate + Pick(seed, id, 2) * 19.0f);
                state = Pick(seed, id + static_cast<unsigned>(static_cast<long long>(change)) * 8191U, 3);
            }
            if (state >= lit) continue;
            const Color4 color = Pick(seed, id, 4) < 0.28f ? windowAlt : window;
            const Float2 at{left + static_cast<float>(column) * pitchX,
                            wall.y + windowHeight + static_cast<float>(row) * pitchY};
            paint.Rect({at.x, at.y, windowWidth, windowHeight}, Fill(color));
        }
}

void DrawRoof(Painter& paint, const FloatRect& wall, float unit, Color4 color, Color4 beacon, bool night, float time,
              unsigned seed) {
    const float style = Pick(seed, 0, 5);
    if (style < 0.22f) { // Antenna
        const float top = wall.y - unit * (0.03f + 0.05f * Pick(seed, 0, 6));
        const float x = wall.x + wall.width * (0.3f + 0.4f * Pick(seed, 0, 7));
        const float width = std::max(unit * 0.0024f, 1.0f / std::max(paint.PixelScale(), 0.001f));
        paint.Line({x, wall.y}, {x, top}, {.width = width, .color = color});
        if (night) {
            const float blink = 0.5f + 0.5f * std::sin(time * 2.2f + Pick(seed, 0, 8) * Pi * 2.0f);
            paint.Circle({x, top}, unit * 0.0035f, Fill(ScaleAlpha(beacon, 0.25f + 0.75f * blink)));
            paint.Glow({x, top}, unit * 0.016f, {.color = ScaleAlpha(beacon, 0.5f * blink)});
        }
    } else if (style < 0.48f) { // Stepped top
        const float inset = wall.width * (0.16f + 0.14f * Pick(seed, 0, 6));
        const float height = unit * (0.02f + 0.03f * Pick(seed, 0, 7));
        paint.Rect({wall.x + inset, wall.y - height, wall.width - inset * 2.0f, height + 1.0f}, Fill(color));
    } else if (style < 0.62f) { // Slanted top
        const float rise = wall.width * (0.2f + 0.3f * Pick(seed, 0, 6));
        const float seat = wall.y + 0.5f; // Under the wall's edge, so no seam shows
        paint.Triangle({wall.x, seat}, {wall.Right(), seat}, {wall.Right(), wall.y - rise}, Fill(color));
    } else if (style < 0.72f) { // Tank
        const float width = wall.width * 0.26f;
        const float height = width * 0.8f;
        const float x = wall.x + wall.width * (0.15f + 0.45f * Pick(seed, 0, 6));
        paint.RoundRect({x, wall.y - height * 1.2f, width, height}, width * 0.2f, Fill(color));
        paint.Rect({x + width * 0.1f, wall.y - height * 0.3f, width * 0.8f, height * 0.4f}, Fill(color));
    }
}
} // namespace

void DrawSkyline(Painter& paint, const FloatRect& box, SkyKind kind, float time, const SkylineLook& look) {
    if (box.Empty()) return;
    const auto clip = paint.Clip(box);
    if (look.sky) DrawSky(paint, box, kind, time, {.seed = look.seed});

    const SkylineColors& own = Kinds[static_cast<std::size_t>(kind)];
    const std::array<Color4, 3> colors = look.buildings.value_or(own.buildings);
    const Color4 window = look.window.value_or(own.window);
    const Color4 windowAlt = look.windowAlt.value_or(own.windowAlt);
    const Color4 haze = look.haze.value_or(own.haze);
    const bool night = kind == SkyKind::Night;
    const float unit = box.height;
    const float pixel = 1.0f / std::max(paint.PixelScale(), 0.001f);
    const float windowWidth = std::max(unit * WindowWidth, pixel * MinWindowPixels);
    const float scrolled = look.scroll + time * look.speed;

    for (std::size_t index = 0; index < Rows.size(); ++index) {
        const BuildingRow& row = Rows[index];
        const unsigned rowSeed = look.seed * 3571U + static_cast<unsigned>(index) * 193U;
        const float offset = scrolled * row.parallax + static_cast<float>(index) * 5.3f;
        const float firstCell = std::floor(offset / row.cell) - 1.0f;
        const int count = static_cast<int>(box.width / unit / row.cell) + 4;
        for (int i = 0; i < count; ++i) {
            const float cell = firstCell + static_cast<float>(i);
            const auto id = static_cast<unsigned>(static_cast<long long>(cell));
            const unsigned seed = rowSeed + id * 37U;
            // Taller blocks gather in districts, so the outline has a shape instead of even noise.
            const float district = 0.55f + 0.45f * SmoothNoise(cell * row.cell * 1.1f, rowSeed);
            const float share = std::pow(Pick(seed, 0, 0), 1.3f) * district;
            const float height = unit * Mix(row.minHeight, row.maxHeight, share);
            const float width = unit * row.cell * (0.74f + 0.42f * Pick(seed, 0, 1));
            const float x = box.x + (cell * row.cell + row.cell * 0.14f * Pick(seed, 0, 2) - offset) * unit;
            const FloatRect wall{x, box.Bottom() - height, width, height};
            DrawRoof(paint, wall, unit, colors[index], own.beacon, night, time, seed);
            paint.Rect(wall, Fill(colors[index]));
            if (row.windows) DrawWindows(paint, wall, windowWidth, window, windowAlt, own.lit, time, seed);
        }
        if (row.haze > 0.0f) {
            const float top = box.Bottom() - unit * row.maxHeight * 0.75f;
            const FloatRect mist{box.x, top, box.width, box.Bottom() - top};
            ShapeStyle style;
            style.gradient = Gradient::Vertical(mist.height, WithAlpha(haze, 0.0f), WithAlpha(haze, row.haze));
            paint.Rect(mist, style);
        }
    }
}
