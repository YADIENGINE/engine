#include "Graphics/Backgrounds/StarField.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include "Graphics/Draw/DrawUtils.h"
#include "Graphics/Paint/Painter.h"
#include "Utilities/Math/Noise.h"
#include "Utilities/Math/Scalar.h"

namespace {
constexpr float FullDetailPixels = 500.0f; // Smaller boxes get fewer stars, or a thumbnail turns to grey noise
constexpr float MinDetail = 0.35f;

struct StarLayer {
    float perSquare; // Stars per square of the box's short side
    float radius;    // Of the short side
    float alpha;
    float speed; // Short sides per second, Drift only
};
constexpr std::array StarLayers{
    StarLayer{170.0f, 0.0011f, 0.55f, 0.006f},
    StarLayer{55.0f, 0.0018f, 0.8f, 0.014f},
    StarLayer{11.0f, 0.0026f, 1.0f, 0.03f},
};
constexpr std::size_t SparkleLayer = 2;

constexpr float WarpPerSquare = 300.0f;
constexpr float WarpSpeed = 0.22f; // Trips from the centre to the edge per second
constexpr float WarpCurve = 2.6f;  // Stars crawl near the centre and rush past the edge

[[nodiscard]] float Pick(unsigned seed, unsigned star, unsigned channel) {
    return Random01(seed * 73856093U + star * 19349663U + channel * 83492791U);
}

[[nodiscard]] Color4 StarColor(const StarFieldLook& look, float choice) {
    if (choice < 0.18f) return Mix(look.star, look.warm, 0.8f);
    if (choice < 0.42f) return Mix(look.star, look.cool, 0.8f);
    return look.star;
}

[[nodiscard]] float Detail(const Painter& paint, const FloatRect& box) {
    return std::clamp(std::min(box.width, box.height) * paint.PixelScale() / FullDetailPixels, MinDetail, 1.0f);
}

void DrawSparkle(Painter& paint, Float2 at, float radius, float twinkle, Color4 color) {
    const float reach = radius * (3.2f + 1.6f * twinkle);
    const float thin = radius * 0.42f;
    const Color4 ray = ScaleAlpha(color, 0.75f);
    DrawDiamond(paint, at, {reach, thin}, Fill(ray));
    DrawDiamond(paint, at, {thin, reach}, Fill(ray));
    paint.Glow(at, radius * 7.0f, {.color = ScaleAlpha(color, 0.4f)});
}

void DrawStars(Painter& paint, const FloatRect& box, bool drift, float time, const StarFieldLook& look) {
    const float unit = std::min(box.width, box.height);
    const float squares = std::max(box.width, box.height) / unit;
    const float amount = Detail(paint, box) * std::max(look.density, 0.0f);
    const float pixel = 1.0f / std::max(paint.PixelScale(), 0.001f);
    for (std::size_t index = 0; index < StarLayers.size(); ++index) {
        const StarLayer& layer = StarLayers[index];
        const auto layerSeed = look.seed + static_cast<unsigned>(index) * 1009U;
        const int count = static_cast<int>(layer.perSquare * squares * amount);
        const float radius = std::max(layer.radius * unit, pixel * 0.55f);
        for (int i = 0; i < count; ++i) {
            const auto star = static_cast<unsigned>(i);
            float x = Pick(layerSeed, star, 0);
            const float y = Pick(layerSeed, star, 1);
            if (look.fade > 0.0f && Pick(layerSeed, star, 2) < y * look.fade) continue;
            if (drift) x = Fract(x - time * look.speed * layer.speed * unit / box.width);
            const Float2 at{box.x + x * box.width, box.y + y * box.height};
            const float rate = 0.7f + Pick(layerSeed, star, 3) * 2.2f;
            const float twinkle = 0.5f + 0.5f * std::sin(time * rate + Pick(layerSeed, star, 4) * Pi * 2.0f);
            const float size = 0.7f + Pick(layerSeed, star, 5) * 0.6f;
            const Color4 color = StarColor(look, Pick(layerSeed, star, 6));
            const Color4 shown = ScaleAlpha(color, layer.alpha * (0.55f + 0.45f * twinkle));
            if (drift && index > 0) { // A trail tells a moving star from a still one, even in a single frame
                const Float2 trail{radius * (index == SparkleLayer ? 16.0f : 8.0f), 0.0f};
                paint.Line(at, at + trail,
                           {.width = radius * size, .color = ScaleAlpha(shown, 0.35f), .cap = StrokeCap::Round});
            }
            if (index == SparkleLayer) DrawSparkle(paint, at, radius * size, twinkle, shown);
            paint.Circle(at, radius * size, Fill(shown));
        }
    }
}

void DrawWarp(Painter& paint, const FloatRect& box, float time, const StarFieldLook& look) {
    const Float2 center = box.Center();
    const float reach = std::hypot(box.width, box.height) * 0.55f;
    const float unit = std::min(box.width, box.height);
    const float squares = std::max(box.width, box.height) / unit;
    const float pixel = 1.0f / std::max(paint.PixelScale(), 0.001f);
    const int count = static_cast<int>(WarpPerSquare * squares * Detail(paint, box) * std::max(look.density, 0.0f));
    paint.Glow(center, unit * 0.22f, {.color = ScaleAlpha(look.cool, 0.16f)});
    for (int i = 0; i < count; ++i) {
        const auto star = static_cast<unsigned>(i);
        const float angle = Pick(look.seed, star, 0) * Pi * 2.0f;
        const float pace = 0.65f + Pick(look.seed, star, 1) * 0.7f;
        const float depth = Fract(Pick(look.seed, star, 2) + time * WarpSpeed * look.speed * pace);
        const float near = std::pow(depth, WarpCurve);
        const Float2 direction = Float2::FromAngle(angle);
        const float from = reach * (0.012f + near);
        const float to = from + reach * (0.008f + 0.34f * near * depth);
        const float width = std::max(unit * (0.0012f + 0.0052f * near), pixel * 0.6f);
        const Color4 color = StarColor(look, Pick(look.seed, star, 3));
        paint.Line(center + direction * from, center + direction * to,
                   {.width = width,
                    .color = ScaleAlpha(color, 0.25f + 0.75f * std::min(depth * 1.4f, 1.0f)),
                    .cap = StrokeCap::Round});
    }
}
} // namespace

void DrawStarField(Painter& paint, const FloatRect& box, StarFieldKind kind, float time, const StarFieldLook& look) {
    if (box.Empty()) return;
    const auto clip = paint.Clip(box);
    if (look.background) {
        ShapeStyle sky;
        sky.gradient = Gradient::Vertical(box.height, look.top, look.bottom);
        paint.Rect(box, sky);
    }
    if (kind == StarFieldKind::Warp)
        DrawWarp(paint, box, time, look);
    else
        DrawStars(paint, box, kind == StarFieldKind::Drift, time, look);
}
