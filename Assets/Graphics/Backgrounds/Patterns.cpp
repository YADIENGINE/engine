#include "Graphics/Backgrounds/Patterns.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include "Graphics/Draw/DrawUtils.h"
#include "Graphics/Paint/Painter.h"
#include "Utilities/Math/Scalar.h"

namespace {
struct PatternColors {
    Color4 top;
    Color4 bottom;
    Color4 mark;
    Color4 bright;
};

// In PatternKind's order.
constexpr std::array<PatternColors, 4> Kinds{
    PatternColors{Rgb(0x171c33), Rgb(0x0d1022), Rgb(0x6e7ab0), Rgb(0xa0acd8)},
    PatternColors{Rgb(0x10122e), Rgb(0x070818), Rgb(0x34418e), Rgb(0x5ee8ff)},
    PatternColors{Rgb(0x11231f), Rgb(0x081311), Rgb(0x2d7a68), Rgb(0x70e0c0)},
    PatternColors{Rgb(0x1c0f30), Rgb(0x0c0618), Rgb(0x4e2e80), Rgb(0xff6ad0)},
};

constexpr float SlideSpeed = 0.18f; // Cells per second
constexpr float DotRadius = 0.17f;  // Of the pitch
constexpr float MinCell = 0.02f;    // Of the short side: finer patterns cost a lot and read as a flat tone
constexpr float Sqrt3 = 1.7320508f;

[[nodiscard]] float Wave(float value) { return 0.5f + 0.5f * std::sin(value); }

// Calls 'each(center, row, column)' for a staggered grid covering 'box', its rows 'pitch.y' apart and every other row
// half a column off.
template <class Each> void ForEachCell(const FloatRect& box, Float2 pitch, Float2 slide, float margin, Each&& each) {
    const Float2 phase{slide.x - std::floor(slide.x / (pitch.x * 2.0f)) * pitch.x * 2.0f,
                       slide.y - std::floor(slide.y / (pitch.y * 2.0f)) * pitch.y * 2.0f};
    const int rows = static_cast<int>((box.height + margin * 2.0f) / pitch.y) + 4;
    const int columns = static_cast<int>((box.width + margin * 2.0f) / pitch.x) + 4;
    for (int row = 0; row < rows; ++row) {
        const float stagger = row % 2 == 0 ? 0.0f : pitch.x * 0.5f;
        for (int column = 0; column < columns; ++column)
            each(Float2{box.x - margin - pitch.x * 2.0f + phase.x + static_cast<float>(column) * pitch.x + stagger,
                        box.y - margin - pitch.y * 2.0f + phase.y + static_cast<float>(row) * pitch.y},
                 row, column);
    }
}

void DrawDots(Painter& paint, const FloatRect& box, float cell, float clock, const PatternColors& colors) {
    const Float2 pitch{cell, cell * 0.5f * Sqrt3};
    const float slide = clock * SlideSpeed * cell;
    ForEachCell(box, pitch, {slide, slide}, cell, [&](Float2 at, int, int) {
        paint.Circle(at, cell * DotRadius, Fill(colors.mark));
    });
}

void DrawDotWave(Painter& paint, const FloatRect& box, float cell, float clock, const PatternColors& colors) {
    const Float2 pitch{cell, cell * 0.5f * Sqrt3};
    const Float2 center = box.Center();
    const float ring = cell * 6.5f;
    ForEachCell(box, pitch, {}, cell, [&](Float2 at, int, int) {
        const Float2 away = at - center;
        const float swell = Wave(std::hypot(away.x, away.y) / ring * Pi * 2.0f - clock * 1.6f);
        const float shaped = swell * swell;
        paint.Circle(at, cell * (0.08f + 0.28f * shaped), Fill(Mix(colors.mark, colors.bright, shaped)));
    });
}

void DrawHexagons(Painter& paint, const FloatRect& box, float cell, float clock, const PatternColors& colors) {
    const float radius = cell / Sqrt3;
    const Float2 pitch{cell, radius * 1.5f};
    const float line = std::max(cell * 0.045f, 1.0f / std::max(paint.PixelScale(), 0.001f));
    const float slide = clock * SlideSpeed * cell;
    ForEachCell(box, pitch, {slide * 0.5f, slide}, radius, [&](Float2 at, int, int) {
        DrawRegularPolygon(paint, at, 6, radius, Radians(30.0f), Outline(line, colors.mark));
    });
}

void DrawHexPulse(Painter& paint, const FloatRect& box, float cell, float clock, const PatternColors& colors) {
    const float radius = cell / Sqrt3;
    const Float2 pitch{cell, radius * 1.5f};
    const float line = std::max(cell * 0.04f, 1.0f / std::max(paint.PixelScale(), 0.001f));
    const Float2 center = box.Center();
    const float ring = cell * 8.0f;
    ForEachCell(box, pitch, {}, radius, [&](Float2 at, int, int) {
        const Float2 away = at - center;
        const float pulse = std::pow(Wave(std::hypot(away.x, away.y) / ring * Pi * 2.0f - clock * 1.2f), 5.0f);
        ShapeStyle style;
        style.fill = WithAlpha(colors.bright, 0.08f + 0.7f * pulse);
        style.stroke = {line, Mix(colors.mark, colors.bright, pulse)};
        DrawRegularPolygon(paint, at, 6, radius * 0.94f, Radians(30.0f), style);
    });
}
} // namespace

void DrawPattern(Painter& paint, const FloatRect& box, PatternKind kind, float time, const PatternLook& look) {
    if (box.Empty()) return;
    const auto clip = paint.Clip(box);
    const PatternColors& own = Kinds[static_cast<std::size_t>(kind)];
    const PatternColors colors{look.top.value_or(own.top), look.bottom.value_or(own.bottom),
                               look.mark.value_or(own.mark), look.bright.value_or(own.bright)};
    if (look.background) {
        ShapeStyle base;
        base.gradient = Gradient::Vertical(box.height, colors.top, colors.bottom);
        paint.Rect(box, base);
    }
    const float cell = std::min(box.width, box.height) * std::max(look.cell, MinCell);
    const float clock = time * look.speed;
    switch (kind) {
    case PatternKind::Dots: DrawDots(paint, box, cell, clock, colors); break;
    case PatternKind::DotWave: DrawDotWave(paint, box, cell, clock, colors); break;
    case PatternKind::Hexagons: DrawHexagons(paint, box, cell, clock, colors); break;
    case PatternKind::HexPulse: DrawHexPulse(paint, box, cell, clock, colors); break;
    }
}
