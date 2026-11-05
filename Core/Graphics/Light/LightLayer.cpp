#include "Graphics/Light/LightLayer.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include "Graphics/Light/LightOccluders.h"
#include "Graphics/Paint/Painter.h"

namespace {
constexpr float LowestResolution = 0.25f;
constexpr float DefaultResolution = 0.5f;
constexpr float DefaultSoftness = 8.0f;
constexpr float SmallestSpread = 1.0f;  // In the image's units: less leaves the steps of its pixels
constexpr float LargestSpread = 24.0f;  // In the image's units
constexpr float OnePassPixels = 8.0f;   // A wider spread, in device pixels, shows the blur's samples
constexpr float SecondPassShare = 0.3f; // A second pass this wide hides them

float ValidResolution(float resolution) {
    return std::isfinite(resolution) ? std::clamp(resolution, LowestResolution, 1.0f) : DefaultResolution;
}

float ValidSoftness(float softness) { return std::isfinite(softness) ? std::max(softness, 0.0f) : DefaultSoftness; }

std::uint8_t AddChannel(std::uint8_t a, std::uint8_t b) {
    return static_cast<std::uint8_t>(std::min(static_cast<int>(a) + static_cast<int>(b), 255));
}
} // namespace

LightLayer::LightLayer(const LightLayerOptions& options)
    : _resolution(ValidResolution(options.resolution))
    , _softness(ValidSoftness(options.softness))
    , _image(Float2{1.0f, 1.0f})
    , _softening(Float2{1.0f, 1.0f}) {}

void LightLayer::Draw(Painter& paint, const LightFrame& frame, const CameraView& view) const {
    if (paint.CurrentTransform() != Affine2::Identity())
        return paint.Reject("LightLayer::Draw must be called outside paint.Camera and paint.Transform");
    const Color4 ambient{AddChannel(frame.ambient.r, frame.flashes.ambient.r),
                         AddChannel(frame.ambient.g, frame.flashes.ambient.g),
                         AddChannel(frame.ambient.b, frame.flashes.ambient.b), 255};
    if (ambient.r == 255 && ambient.g == 255 && ambient.b == 255) return; // Nothing to darken, and light adds nothing
    const FloatRect bounds = paint.Bounds();
    _image.Resize(bounds.Size() * _resolution);
    bool shadowed = false;
    {
        auto into = paint.Target(_image, ambient);
        auto scale = paint.Transform(Affine2::Scale(_resolution));
        auto camera = paint.Camera(view);
        const auto draw = [&](const LightSource& light) {
            _blocking.clear();
            if (light.shadows && light.radius > 0.0f)
                for (const LightOccluders* occluders : frame.occluders)
                    if (occluders) occluders->AppendBlocking(light.at, light.radius, _blocking);
            if (_blocking.empty()) {
                paint.Light(light);
                return;
            }
            paint.Light(light, _visibility.From(light.at, light.radius, _blocking));
            shadowed = true;
        };
        for (const LightSource& light : frame.lights)
            draw(light);
        for (const LightSource& light : frame.flashes.lights)
            draw(light);
    }
    // Only a shadow has an edge to soften: a light's own falloff is smooth at any size.
    if (shadowed && _softness > 0.0f) Soften(paint, bounds.Size() * _resolution, _softness * view.zoom * _resolution);
    std::optional<PaintScope> clip;
    if (!view.viewport.Empty()) clip.emplace(paint.Clip(view.viewport));
    auto multiply = paint.Blending(Blend::Multiply);
    paint.Image(_image, bounds);
}

void LightLayer::Soften(Painter& paint, Float2 size, float spread) const {
    const float wide = std::clamp(spread, SmallestSpread, LargestSpread);
    const FloatRect whole{0.0f, 0.0f, size.x, size.y};
    _softening.Resize(size);
    float pixels = 1.0f;
    const auto cross = [&](float along) {
        {
            auto into = paint.Target(_softening, std::nullopt);
            pixels = paint.PixelScale();
            paint.BlurredImage(_image, whole, {along, 0.0f});
        }
        auto into = paint.Target(_image, std::nullopt);
        paint.BlurredImage(_softening, whole, {0.0f, along});
    };
    cross(wide);
    if (wide * pixels > OnePassPixels) cross(wide * SecondPassShare);
}
