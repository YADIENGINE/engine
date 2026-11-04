#include "Graphics/Backgrounds/Sky.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "Graphics/Backgrounds/StarField.h"
#include "Graphics/Paint/Painter.h"
#include "Utilities/Math/Noise.h"
#include "Utilities/Math/Scalar.h"

namespace {
struct SkyColors {
    std::array<Color4, 4> bands;
    Color4 body;
    Color4 bodyEdge; // Foot of the sun's gradient; the moon's craters
    Color4 halo;
    Color4 wisp;
    Float2 bodyAt;
    float bodySize;
    int wisps;
    float stars;
};

// In SkyKind's order.
constexpr std::array<SkyColors, 3> Kinds{
    SkyColors{{Rgb(0x2a6fd0), Rgb(0x4d97e6), Rgb(0x8cc4f0), Rgb(0xd6edf8)},
              Rgb(0xffffff),
              Rgb(0xfff8d0),
              Rgb(0xfff0c0),
              Rgb(0xffffff, 95),
              {0.76f, 0.2f},
              0.062f,
              7,
              0.0f},
    SkyColors{{Rgb(0x2b2a6e), Rgb(0x8a3a86), Rgb(0xee6a6c), Rgb(0xffbd66)},
              Rgb(0xfff0a8),
              Rgb(0xff7e4a),
              Rgb(0xff9a5a),
              Rgb(0x6a2a70, 120),
              {0.5f, 0.74f},
              0.13f,
              11,
              0.25f},
    SkyColors{{Rgb(0x050a20), Rgb(0x0b1538), Rgb(0x152450), Rgb(0x2a3c70)},
              Rgb(0xf6f2da),
              Rgb(0xd8d6c4),
              Rgb(0xb4c8ff),
              Rgb(0x3a4c80, 70),
              {0.74f, 0.22f},
              0.07f,
              5,
              0.7f},
};

[[nodiscard]] const SkyColors& ColorsOf(SkyKind kind) { return Kinds[static_cast<std::size_t>(kind)]; }

[[nodiscard]] float Pick(unsigned seed, unsigned index, unsigned channel) {
    return Random01(seed * 40503U + index * 2654435761U + channel * 19349663U);
}

void DrawWisps(Painter& paint, const FloatRect& box, Color4 color, float bodyY, int count, float time, unsigned seed) {
    const float unit = std::min(box.width, box.height);
    for (int i = 0; i < count; ++i) {
        const auto index = static_cast<unsigned>(i);
        // Streaks stay near a low sun, passing in front of it, and high up otherwise.
        const float band = bodyY > 0.5f ? 0.42f + 0.46f * Pick(seed, index, 0) : 0.06f + 0.42f * Pick(seed, index, 0);
        const float length = box.width * (0.14f + 0.24f * Pick(seed, index, 1));
        const float start = Fract(Pick(seed, index, 2) + time * 0.002f * (0.5f + Pick(seed, index, 3)));
        const float x = box.x - length + start * (box.width + length * 2.0f);
        const float y = box.y + band * box.height;
        const float width = unit * (0.005f + 0.011f * Pick(seed, index, 4));
        // Thin ellipses taper to nothing at both ends, as a streak of cloud does.
        paint.Ellipse({x, y}, {length * 0.5f, width * 0.6f}, Fill(color));
        paint.Ellipse({x + length * 0.1f, y - width * 0.8f}, {length * 0.3f, width * 0.45f},
                      Fill(ScaleAlpha(color, 0.75f)));
        paint.Ellipse({x - length * 0.16f, y + width * 0.7f}, {length * 0.24f, width * 0.35f},
                      Fill(ScaleAlpha(color, 0.6f)));
    }
}

void DrawSun(Painter& paint, Float2 at, float radius, Color4 body, Color4 edge, Color4 halo) {
    paint.Glow(at, radius * 7.0f, {.color = ScaleAlpha(halo, 0.26f)});
    paint.Glow(at, radius * 2.4f, {.color = ScaleAlpha(halo, 0.32f)});
    ShapeStyle disc;
    disc.gradient = Gradient::Vertical(radius * 2.0f, body, edge);
    paint.Circle(at, radius, disc);
}

void DrawMoon(Painter& paint, Float2 at, float radius, Color4 body, Color4 crater, Color4 halo) {
    paint.Glow(at, radius * 5.0f, {.color = ScaleAlpha(halo, 0.34f)});
    paint.Circle(at, radius, Fill(body));
    const ShapeStyle pit = Fill(ScaleAlpha(crater, 0.85f));
    paint.Circle(at + Float2{-0.34f, -0.26f} * radius, radius * 0.22f, pit);
    paint.Circle(at + Float2{0.28f, -0.42f} * radius, radius * 0.12f, pit);
    paint.Circle(at + Float2{0.18f, 0.2f} * radius, radius * 0.3f, pit);
    paint.Circle(at + Float2{-0.36f, 0.42f} * radius, radius * 0.14f, pit);
    paint.Circle(at + Float2{0.62f, -0.04f} * radius, radius * 0.09f, pit);
}
} // namespace

void DrawGradientBands(Painter& paint, const FloatRect& box, std::span<const Color4> colors) {
    if (box.Empty() || colors.size() < 2) return;
    std::vector<Float2> points;
    std::vector<Color4> shades;
    std::vector<std::uint32_t> indices;
    points.reserve(colors.size() * 2);
    shades.reserve(colors.size() * 2);
    indices.reserve((colors.size() - 1) * 6);
    for (std::size_t i = 0; i < colors.size(); ++i) {
        const float y = box.y + box.height * static_cast<float>(i) / static_cast<float>(colors.size() - 1);
        points.push_back({box.x, y});
        points.push_back({box.Right(), y});
        shades.push_back(colors[i]);
        shades.push_back(colors[i]);
        if (i == 0) continue;
        const auto top = static_cast<std::uint32_t>(i * 2 - 2);
        for (const std::uint32_t corner : {top, top + 1, top + 2, top + 1, top + 3, top + 2})
            indices.push_back(corner);
    }
    paint.Mesh(points, shades, indices);
}

void DrawSky(Painter& paint, const FloatRect& box, SkyKind kind, float time, const SkyLook& look) {
    if (box.Empty()) return;
    const auto clip = paint.Clip(box);
    const SkyColors& own = ColorsOf(kind);
    const std::array bands{look.top.value_or(own.bands[0]), look.upper.value_or(own.bands[1]),
                           look.lower.value_or(own.bands[2]), look.horizon.value_or(own.bands[3])};
    DrawGradientBands(paint, box, bands);

    const float stars = look.stars.value_or(own.stars);
    if (stars > 0.0f)
        DrawStarField(paint, box, StarFieldKind::Still, time,
                      {.background = false,
                       .density = stars,
                       .fade = kind == SkyKind::Night ? 0.85f : 2.4f,
                       .seed = look.seed + 11U});

    const Float2 bodyShare = look.bodyAt.value_or(own.bodyAt);
    const float radius = look.bodySize.value_or(own.bodySize) * std::min(box.width, box.height);
    const Float2 at{box.x + bodyShare.x * box.width, box.y + bodyShare.y * box.height};
    const Color4 body = look.body.value_or(own.body);
    const int wisps = static_cast<int>(static_cast<float>(own.wisps) * std::max(look.wisps.value_or(1.0f), 0.0f));
    const bool lowBody = bodyShare.y > 0.5f; // A low sun shines through its streaks; a high one is above them
    if (!lowBody) DrawWisps(paint, box, own.wisp, bodyShare.y, wisps, time, look.seed);
    if (radius > 0.0f) {
        if (kind == SkyKind::Night)
            DrawMoon(paint, at, radius, body, own.bodyEdge, own.halo);
        else
            DrawSun(paint, at, radius, body, look.body ? body : own.bodyEdge, own.halo);
    }
    if (lowBody) DrawWisps(paint, box, own.wisp, bodyShare.y, wisps, time, look.seed);
}
