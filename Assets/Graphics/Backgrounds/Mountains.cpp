#include "Graphics/Backgrounds/Mountains.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>
#include "Graphics/Paint/Painter.h"
#include "Utilities/Random/Noise.h"
#include "Utilities/Random/NoiseField.h"
#include "Utilities/Math/Scalar.h"

namespace {
struct MountainColors {
    std::array<Color4, 4> ridges;
    Color4 snow;
    Color4 haze;
};

// In SkyKind's order.
constexpr std::array<MountainColors, 3> Kinds{
    MountainColors{{Rgb(0x9fbbdb), Rgb(0x6f92bd), Rgb(0x47806e), Rgb(0x24523f)}, Rgb(0xf6fafe), Rgb(0xdcecf6)},
    MountainColors{{Rgb(0xc26f8e), Rgb(0x8c4c86), Rgb(0x55306e), Rgb(0x2a1a48)}, Rgb(0xffd2b4), Rgb(0xff9f70)},
    MountainColors{{Rgb(0x2c3d6c), Rgb(0x1f2d55), Rgb(0x141f40), Rgb(0x0a1028)}, Rgb(0x93a8d2), Rgb(0x3a4f86)},
};

struct RidgeShape {
    float base;      // Foot line, as a share of the box height from its bottom
    float amplitude; // Peaks reach this much above it
    float frequency; // Large peaks per box height
    float parallax;  // Share of the scroll this ridge follows
    float snowLine;  // Share of the ridge's height above which it has snow; 1: none
    float haze;      // Mist in front of the ridge
};
constexpr std::array<RidgeShape, 4> Ridges{
    RidgeShape{0.3f, 0.62f, 1.3f, 0.18f, 0.58f, 0.5f},
    RidgeShape{0.22f, 0.5f, 1.9f, 0.36f, 0.64f, 0.42f},
    RidgeShape{0.13f, 0.34f, 2.6f, 0.62f, 1.0f, 0.3f},
    RidgeShape{0.04f, 0.2f, 3.6f, 1.0f, 1.0f, 0.0f},
};
constexpr std::size_t TreeRidge = 3;
constexpr float StepShare = 0.006f; // Distance between ridge points, of the box height
constexpr float TreeSpacing = 0.021f;

// 0..1 along straight slopes between random heights.
[[nodiscard]] float SlopeNoise(float x, unsigned seed) {
    const float cell = std::floor(x);
    const auto at = static_cast<unsigned>(static_cast<long long>(cell));
    return Mix(Random01(at * 2654435761U + seed), Random01((at + 1U) * 2654435761U + seed), x - cell);
}

// 0..1. Straight slopes at every scale give angular peaks; smooth noise would give hills.
[[nodiscard]] float RidgeHeight(float x, unsigned seed) {
    float sum = 0.0f;
    float weight = 0.52f;
    float frequency = 1.0f;
    for (unsigned octave = 0; octave < 5U; ++octave) {
        sum += SlopeNoise(x * frequency, seed + octave * 101U) * weight;
        weight *= 0.5f;
        frequency *= 2.3f;
    }
    // The sum stays under 1; dividing by less would flatten the highest peaks into plateaus.
    return std::clamp(sum, 0.0f, 1.0f);
}

void DrawSnow(Painter& paint, std::span<const Float2> crest, std::span<const float> heights, float snowLine,
              float depth, float minThickness, Color4 color, std::vector<Float2>& shape) {
    constexpr float tearFrequency = 11.0f;
    std::size_t start = 0;
    while (start < heights.size()) {
        if (heights[start] < snowLine) {
            ++start;
            continue;
        }
        std::size_t end = start;
        while (end + 1 < heights.size() && heights[end + 1] >= snowLine) ++end;
        if (end - start >= 4) {
            shape.assign(crest.begin() + static_cast<std::ptrdiff_t>(start),
                         crest.begin() + static_cast<std::ptrdiff_t>(end) + 1);
            // Back along a torn lower edge under the crest. A share of the height above the snow line makes the cap
            // follow its peak; all of it would rule a flat line under it. A few pixels at least, or a small view shows
            // the caps as thin broken lines.
            for (std::size_t i = end; i-- > start + 1;) {
                const float tear = 0.55f + 0.3f * Noise(NoiseSettings{.seed = 77U}, heights[i] * tearFrequency);
                const float thickness = std::max((heights[i] - snowLine) * depth * tear, minThickness);
                shape.push_back({crest[i].x, crest[i].y + thickness});
            }
            paint.Polygon(shape, Fill(color));
        }
        start = end + 1;
    }
}

void DrawTrees(Painter& paint, const FloatRect& box, const RidgeShape& ridge, float offset, unsigned seed,
               Color4 color) {
    const float spacing = box.height * TreeSpacing;
    const float first = std::floor(offset / TreeSpacing);
    const int count = static_cast<int>(box.width / spacing) + 3;
    for (int i = 0; i < count; ++i) {
        const float index = first + static_cast<float>(i);
        const auto tree = static_cast<unsigned>(static_cast<long long>(index));
        const float jitter = Random01(tree * 7919U + seed);
        const float world = index * TreeSpacing + jitter * TreeSpacing * 0.6f;
        const float x = box.x + (world - offset) * box.height;
        const float crest = RidgeHeight(world * ridge.frequency, seed);
        const float y = box.Bottom() - box.height * (ridge.base + ridge.amplitude * crest) + box.height * 0.004f;
        const float height = box.height * (0.035f + 0.04f * Random01(tree * 104729U + seed));
        const float half = height * 0.27f;
        paint.Triangle({x - half, y}, {x, y - height * 0.72f}, {x + half, y}, Fill(color));
        paint.Triangle({x - half * 0.72f, y - height * 0.4f}, {x, y - height}, {x + half * 0.72f, y - height * 0.4f},
                       Fill(color));
    }
}
} // namespace

void DrawMountains(Painter& paint, const FloatRect& box, SkyKind kind, float time, const MountainsLook& look) {
    if (box.Empty()) return;
    const auto clip = paint.Clip(box);
    if (look.sky) DrawSky(paint, box, kind, time, {.seed = look.seed});

    const MountainColors& own = Kinds[static_cast<std::size_t>(kind)];
    const std::array<Color4, 4> colors = look.ridges.value_or(own.ridges);
    const Color4 snow = look.snow.value_or(own.snow);
    const Color4 haze = look.haze.value_or(own.haze);
    const float scrolled = look.scroll + time * look.speed;
    const float aspect = box.width / box.height;
    const float minThickness = 3.0f / std::max(paint.PixelScale(), 0.001f);

    std::vector<Float2> crest;
    std::vector<float> heights;
    std::vector<Float2> shape;
    for (std::size_t index = 0; index < Ridges.size(); ++index) {
        const RidgeShape& ridge = Ridges[index];
        const unsigned seed = look.seed * 4099U + static_cast<unsigned>(index) * 613U;
        const float offset = scrolled * ridge.parallax + static_cast<float>(index) * 3.7f;
        // Points sit on a fixed grid of the world, or crests would shimmer while scrolling.
        const float first = std::floor(offset / StepShare);
        const int count = static_cast<int>(aspect / StepShare) + 3;
        crest.clear();
        heights.clear();
        for (int i = 0; i < count; ++i) {
            const float world = (first + static_cast<float>(i)) * StepShare;
            const float height = RidgeHeight(world * ridge.frequency, seed);
            heights.push_back(height);
            crest.push_back({box.x + (world - offset) * box.height,
                             box.Bottom() - box.height * (ridge.base + ridge.amplitude * height)});
        }
        shape = crest;
        shape.push_back({crest.back().x, box.Bottom()});
        shape.push_back({crest.front().x, box.Bottom()});
        paint.Polygon(shape, Fill(colors[index]));
        if (ridge.snowLine < 1.0f)
            DrawSnow(paint, crest, heights, ridge.snowLine, box.height * ridge.amplitude, minThickness, snow, shape);
        if (index == TreeRidge) DrawTrees(paint, box, ridge, offset, seed, colors[index]);
        if (ridge.haze > 0.0f) {
            const float top = box.Bottom() - box.height * (ridge.base + ridge.amplitude * 0.35f);
            const FloatRect mist{box.x, top, box.width, box.Bottom() - top};
            ShapeStyle style;
            style.gradient = Gradient::Vertical(mist.height, WithAlpha(haze, 0.0f), WithAlpha(haze, ridge.haze));
            paint.Rect(mist, style);
        }
    }
}
