#include "Graphics/Backgrounds/Clouds.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include "Graphics/Paint/Painter.h"
#include "Utilities/Random/Noise.h"
#include "Utilities/Math/Scalar.h"

namespace {
struct CloudTones {
    Color4 light;
    Color4 body;
    Color4 shade;
    Color4 far;
};

// In SkyKind's order.
constexpr std::array<CloudTones, 3> Kinds{
    CloudTones{Rgb(0xffffff), Rgb(0xeef6fc), Rgb(0xb5cfe6), Rgb(0xa5d0ef)},
    CloudTones{Rgb(0xffe6b0), Rgb(0xf79a7c), Rgb(0x9a4a84), Rgb(0xee6f70)},
    CloudTones{Rgb(0x8ea2d4), Rgb(0x4a5c92), Rgb(0x222f5c), Rgb(0x1c2c58)},
};

struct CloudLayer {
    float perSquare; // Clouds per square of the box's short side
    float width;     // Of the short side
    float top;       // Base line's range, as a share of the box height
    float bottom;
    float speed; // Short sides per second
    float fade;  // Towards the far colour
};
// Nearest last. Near clouds pass high overhead; far ones sink to the horizon.
constexpr std::array<CloudLayer, 3> Layers{
    CloudLayer{3.0f, 0.2f, 0.62f, 0.84f, 0.004f, 0.62f},
    CloudLayer{1.9f, 0.34f, 0.38f, 0.62f, 0.008f, 0.3f},
    CloudLayer{1.25f, 0.6f, 0.2f, 0.42f, 0.015f, 0.0f},
};

constexpr int MinPuffs = 5;
constexpr int PuffChoices = 3;
constexpr float CloudHeight = 0.42f; // Of its width

[[nodiscard]] float Pick(unsigned seed, unsigned cloud, unsigned channel) {
    return Random01(seed * 2147483647U + cloud * 48271U + channel * 16807U);
}

// Every puff sits on the same base line, which keeps the underside flat.
template <class Each> void ForEachPuff(float width, float height, unsigned seed, Each&& each) {
    const int count = MinPuffs + static_cast<int>(Hash(seed) % PuffChoices);
    for (int i = 0; i < count; ++i) {
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
        const float envelope = std::pow(std::sin(t * Pi), 0.8f);
        const float size = 0.82f + 0.36f * Pick(seed, static_cast<unsigned>(i), 7);
        each((t - 0.5f) * width * 0.8f, height * (0.2f + 0.42f * envelope) * size, envelope);
    }
}

void DrawCloud(Painter& paint, Float2 base, float width, unsigned seed, const CloudTones& tones) {
    const float height = width * CloudHeight;
    const float foot = height * 0.2f;
    const FloatRect shadeBar{base.x - width * 0.46f, base.y - foot, width * 0.92f, foot};
    paint.RoundRect(shadeBar, foot * 0.5f, Fill(tones.shade));
    ForEachPuff(width, height, seed, [&](float x, float radius, float) {
        paint.Circle({base.x + x, base.y - radius}, radius, Fill(tones.shade));
    });
    const float lift = height * 0.075f;
    const FloatRect bodyBar{base.x - width * 0.43f, base.y - foot - lift, width * 0.86f, foot};
    paint.RoundRect(bodyBar, foot * 0.5f, Fill(tones.body));
    ForEachPuff(width, height, seed, [&](float x, float radius, float) {
        paint.Circle({base.x + x - radius * 0.03f, base.y - radius - lift}, radius * 0.93f, Fill(tones.body));
    });
    ForEachPuff(width, height, seed, [&](float x, float radius, float envelope) {
        if (envelope < 0.7f) return; // On the low puffs at the ends, a highlight reads as a separate ball
        const float small = radius * 0.52f;
        paint.Circle({base.x + x - radius * 0.22f, base.y - radius - lift - radius * 0.34f}, small, Fill(tones.light));
    });
}
} // namespace

void DrawClouds(Painter& paint, const FloatRect& box, SkyKind kind, float time, const CloudsLook& look) {
    if (box.Empty()) return;
    const auto clip = paint.Clip(box);
    if (look.sky) DrawSky(paint, box, kind, time, {.seed = look.seed});

    const CloudTones& own = Kinds[static_cast<std::size_t>(kind)];
    const CloudTones tones{look.light.value_or(own.light), look.body.value_or(own.body), look.shade.value_or(own.shade),
                           look.far.value_or(own.far)};
    const float unit = std::min(box.width, box.height);
    const float squares = box.width / unit;
    for (std::size_t index = 0; index < Layers.size(); ++index) {
        const CloudLayer& layer = Layers[index];
        const CloudTones faded{Mix(tones.light, tones.far, layer.fade), Mix(tones.body, tones.far, layer.fade),
                               Mix(tones.shade, tones.far, layer.fade), tones.far};
        const unsigned layerSeed = look.seed * 811U + static_cast<unsigned>(index) * 37U;
        const float amount = layer.perSquare * squares * std::max(look.amount, 0.0f);
        const int count = std::max(1, static_cast<int>(std::lround(amount)));
        for (int i = 0; i < count; ++i) {
            const auto cloud = static_cast<unsigned>(i);
            const float width = unit * layer.width * (0.75f + 0.5f * Pick(layerSeed, cloud, 0));
            // One slot per cloud keeps them spread out; the jitter hides the grid.
            const float slot = (static_cast<float>(i) + Pick(layerSeed, cloud, 1) * 0.7f) / static_cast<float>(count);
            const float travel = box.width + width * 1.2f;
            const float x =
                box.x - width * 0.6f + Fract(slot + time * look.speed * layer.speed * unit / travel) * travel;
            // A cloud never loses its top to the edge of the box.
            const float y = std::max(box.y + box.height * Mix(layer.top, layer.bottom, Pick(layerSeed, cloud, 2)),
                                     box.y + width * CloudHeight * 1.05f);
            DrawCloud(paint, {x, y}, width, layerSeed + cloud * 29U, faded);
        }
    }
}
