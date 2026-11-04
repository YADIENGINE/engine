#pragma once
#include "Utilities/Geometry/Float2.h"

// Integer 2D point, size, offset or grid coordinate. Header-only because it is trivial and constexpr.
struct Vec2 {
    int x = 0;
    int y = 0;

    constexpr Vec2 operator+(Vec2 other) const { return {x + other.x, y + other.y}; }
    constexpr Vec2 operator-(Vec2 other) const { return {x - other.x, y - other.y}; }
    constexpr Vec2 operator*(int scalar) const { return {x * scalar, y * scalar}; }

    constexpr Vec2& operator+=(Vec2 other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    constexpr bool operator==(const Vec2&) const = default;
};

// The same point in continuous coordinates (for drawing).
[[nodiscard]] constexpr Float2 ToFloat2(Vec2 v) { return {static_cast<float>(v.x), static_cast<float>(v.y)}; }
