#pragma once
#include <algorithm>
#include "Utilities/Geometry/Affine2.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/FloatRect.h"

// What one camera shows, for one frame: the world point 'target' appears at the canvas point 'offset', scaled by 'zoom'
// and turned by 'rotation' (radians, clockwise) around it. 'viewport' is the part of the canvas it draws on; empty
// means the whole target, unclipped. Draw the world through it with paint.Camera(view). Camera2D makes one each frame;
// it is also plain data a cinematic shot or a test can build. Header-only.
struct CameraView {
    Float2 offset{};
    Float2 target{};
    float rotation = 0.0f;
    float zoom = 1.0f;
    FloatRect viewport{};

    [[nodiscard]] Affine2 Transform() const { return Affine2::Camera(target, offset, zoom, rotation); }
    [[nodiscard]] Float2 ToScreen(Float2 world) const { return Transform().Apply(world); }
    [[nodiscard]] Float2 ToWorld(Float2 screen) const { return Transform().Inverse().Apply(screen); }

    // The world rectangle the viewport shows, ignoring rotation; empty without a viewport or zoom.
    [[nodiscard]] constexpr FloatRect VisibleArea() const {
        if (viewport.Empty() || !(zoom > 0.0f)) return {};
        return {target.x - (offset.x - viewport.x) / zoom, target.y - (offset.y - viewport.y) / zoom,
                viewport.width / zoom, viewport.height / zoom};
    }
    // The same view moved as little as possible so that what it shows stays inside 'limits'. On an axis where the view
    // is larger than the limits, they are centred. Rotation is ignored.
    [[nodiscard]] constexpr CameraView ConfinedTo(const FloatRect& limits) const {
        const FloatRect visible = VisibleArea();
        if (visible.Empty()) return *this;
        CameraView confined = *this;
        const auto axis = [](float target, float low, float size, float limitLow, float limitSize) {
            const float before = target - low; // World distance from the visible edge to the target
            if (size >= limitSize) return limitLow + (limitSize - size) * 0.5f + before;
            return std::clamp(low, limitLow, limitLow + limitSize - size) + before;
        };
        confined.target.x = axis(target.x, visible.x, visible.width, limits.x, limits.width);
        confined.target.y = axis(target.y, visible.y, visible.height, limits.y, limits.height);
        return confined;
    }
};
