#pragma once
#include <cmath>
#include "Utilities/Float2.h"

// 2D affine transform (rotation / scale / skew + translation), y down, angles in radians clockwise on screen:
//   x' = a * x + c * y + tx
//   y' = b * x + d * y + ty
// 'outer * inner' applies 'inner' first. Header-only.
struct Affine2 {
    float a = 1.0f;
    float b = 0.0f;
    float c = 0.0f;
    float d = 1.0f;
    float tx = 0.0f;
    float ty = 0.0f;

    [[nodiscard]] static constexpr Affine2 Identity() { return {}; }
    [[nodiscard]] static constexpr Affine2 Translate(Float2 offset) {
        return {1.0f, 0.0f, 0.0f, 1.0f, offset.x, offset.y};
    }
    [[nodiscard]] static constexpr Affine2 Scale(Float2 scale, Float2 pivot = {}) {
        return {scale.x, 0.0f, 0.0f, scale.y, pivot.x - pivot.x * scale.x, pivot.y - pivot.y * scale.y};
    }
    [[nodiscard]] static constexpr Affine2 Scale(float scale, Float2 pivot = {}) {
        return Scale({scale, scale}, pivot);
    }
    [[nodiscard]] static Affine2 Rotate(float radians, Float2 pivot = {}) {
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        return {cosine,
                sine,
                -sine,
                cosine,
                pivot.x - cosine * pivot.x + sine * pivot.y,
                pivot.y - sine * pivot.x - cosine * pivot.y};
    }
    // 2D camera: world point 'target' appears at 'screen', scaled by 'zoom' and rotated by 'radians' around it.
    [[nodiscard]] static Affine2 Camera(Float2 target, Float2 screen, float zoom, float radians = 0.0f) {
        return Translate(screen) * Rotate(radians) * Scale(zoom) * Translate(-target);
    }

    [[nodiscard]] constexpr Float2 Apply(Float2 point) const {
        return {a * point.x + c * point.y + tx, b * point.x + d * point.y + ty};
    }
    // Direction / size vector: translation is not applied.
    [[nodiscard]] constexpr Float2 ApplyVector(Float2 vector) const {
        return {a * vector.x + c * vector.y, b * vector.x + d * vector.y};
    }
    [[nodiscard]] constexpr float Determinant() const { return a * d - b * c; }
    // Inverse transform; identity when the transform is not invertible (zero scale).
    [[nodiscard]] constexpr Affine2 Inverse() const {
        const float det = Determinant();
        if (det == 0.0f) return {};
        const float inv = 1.0f / det;
        return {d * inv, -b * inv, -c * inv, a * inv, (c * ty - d * tx) * inv, (b * tx - a * ty) * inv};
    }
    // Geometric mean of the axis scales: how many target units one local unit covers on average.
    [[nodiscard]] float UniformScale() const { return std::sqrt(std::abs(Determinant())); }
    // True when the transform maps axis-aligned rectangles to axis-aligned rectangles without mirroring.
    [[nodiscard]] constexpr bool AxisAligned() const { return b == 0.0f && c == 0.0f && a > 0.0f && d > 0.0f; }

    friend constexpr Affine2 operator*(const Affine2& outer, const Affine2& inner) {
        return {outer.a * inner.a + outer.c * inner.b,
                outer.b * inner.a + outer.d * inner.b,
                outer.a * inner.c + outer.c * inner.d,
                outer.b * inner.c + outer.d * inner.d,
                outer.a * inner.tx + outer.c * inner.ty + outer.tx,
                outer.b * inner.tx + outer.d * inner.ty + outer.ty};
    }
    friend constexpr bool operator==(const Affine2&, const Affine2&) = default;
};
