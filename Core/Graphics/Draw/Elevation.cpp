#include "Graphics/Draw/Elevation.h"
#include <algorithm>
#include <cmath>
#include "Utilities/Geometry/Affine2.h"

namespace {
bool Valid(const GroundShadow& shadow) {
    return std::isfinite(shadow.radius) && shadow.radius >= 0.0f && std::isfinite(shadow.flatten) &&
           shadow.flatten > 0.0f && std::isfinite(shadow.fadeHeight) && shadow.fadeHeight > 0.0f;
}
} // namespace

void DrawGroundShadow(Painter& paint, Float2 ground, float height, const GroundShadow& shadow) {
    if (!std::isfinite(height) || height < 0.0f) return paint.Reject("DrawGroundShadow: height must be 0 or more");
    if (!Valid(shadow)) return paint.Reject("DrawGroundShadow: invalid shadow look");
    const float risen = std::min(height / shadow.fadeHeight, 1.0f);
    if (risen >= 1.0f || shadow.radius == 0.0f) return;
    const float radius = shadow.radius * (1.0f - 0.5f * risen);
    paint.Ellipse(ground, {radius, radius * shadow.flatten}, Fill(ScaleAlpha(shadow.color, 1.0f - risen)));
}

PaintScope Raised(Painter& paint, float height) {
    if (!std::isfinite(height)) {
        paint.Reject("Raised: height must be finite");
        return paint.Transform({});
    }
    return paint.Transform(Affine2::Translate({0.0f, -height}));
}
