#pragma once
#include "Utilities/Affine2.h"
#include "Utilities/Float2.h"

// 2D camera: the world point 'target' appears at the screen point 'offset', scaled by 'zoom' and turned by
// 'rotation' (radians, clockwise) around it. Draw through it with paint.Transform(camera.Transform()). Header-only.
struct CameraView {
    Float2 offset{};
    Float2 target{};
    float rotation = 0.0f;
    float zoom = 1.0f;

    [[nodiscard]] Affine2 Transform() const { return Affine2::Camera(target, offset, zoom, rotation); }
    [[nodiscard]] Float2 ToScreen(Float2 world) const { return Transform().Apply(world); }
    [[nodiscard]] Float2 ToWorld(Float2 screen) const { return Transform().Inverse().Apply(screen); }
};
