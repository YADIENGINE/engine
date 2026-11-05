#pragma once
#include <algorithm>
#include <array>
#include "Camera/CameraView.h"
#include "Graphics/Paint/Painter.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/FloatRect.h"

// How much of the world one repeat of a field covers.
inline constexpr Float2 AtmosphereTile{1536.0f, 1024.0f};

// The view a layer of the air draws through: an empty viewport means the whole target.
[[nodiscard]] inline CameraView AtmosphereLayerView(const Painter& paint, const CameraView& view, float parallax) {
    CameraView layer = view.Parallaxed(parallax);
    if (layer.viewport.Empty()) layer.viewport = paint.Bounds();
    return layer;
}

// The world rectangle around what the view shows, rotation included.
[[nodiscard]] inline FloatRect AtmosphereSeenArea(const CameraView& view) {
    const FloatRect& box = view.viewport;
    const std::array corners{view.ToWorld({box.x, box.y}), view.ToWorld({box.x + box.width, box.y}),
                             view.ToWorld({box.x, box.y + box.height}),
                             view.ToWorld({box.x + box.width, box.y + box.height})};
    Float2 low = corners[0];
    Float2 high = corners[0];
    for (const Float2 corner : corners) {
        low = {std::min(low.x, corner.x), std::min(low.y, corner.y)};
        high = {std::max(high.x, corner.x), std::max(high.y, corner.y)};
    }
    return {low.x, low.y, high.x - low.x, high.y - low.y};
}

// A view larger than a tile shows a share of the pieces, so that the cost has a ceiling however far a camera zooms out.
[[nodiscard]] inline float AtmosphereThinning(const FloatRect& seen) {
    const float area = seen.width * seen.height;
    const float tile = AtmosphereTile.x * AtmosphereTile.y;
    return area > tile ? tile / area : 1.0f;
}
