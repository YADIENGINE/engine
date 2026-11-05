#include "Graphics/Light/LightLayer.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include "Graphics/Paint/Painter.h"

namespace {
constexpr float LowestResolution = 0.25f;
constexpr float DefaultResolution = 0.5f;

float ValidResolution(float resolution) {
    return std::isfinite(resolution) ? std::clamp(resolution, LowestResolution, 1.0f) : DefaultResolution;
}
} // namespace

LightLayer::LightLayer(const LightLayerOptions& options)
    : _resolution(ValidResolution(options.resolution))
    , _image(Float2{1.0f, 1.0f}) {}

void LightLayer::Draw(Painter& paint, const LightFrame& frame, const CameraView& view) const {
    if (paint.CurrentTransform() != Affine2::Identity())
        return paint.Reject("LightLayer::Draw must be called outside paint.Camera and paint.Transform");
    const Color4 ambient{frame.ambient.r, frame.ambient.g, frame.ambient.b, 255};
    if (ambient.r == 255 && ambient.g == 255 && ambient.b == 255) return; // Nothing to darken, and light adds nothing
    const FloatRect bounds = paint.Bounds();
    _image.Resize(bounds.Size() * _resolution);
    {
        auto into = paint.Target(_image, ambient);
        auto scale = paint.Transform(Affine2::Scale(_resolution));
        auto camera = paint.Camera(view);
        for (const LightSource& light : frame.lights)
            paint.Light(light);
    }
    std::optional<PaintScope> clip;
    if (!view.viewport.Empty()) clip.emplace(paint.Clip(view.viewport));
    auto multiply = paint.Blending(Blend::Multiply);
    paint.Image(_image, bounds);
}
