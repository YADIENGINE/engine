#pragma once
#include <cmath>

// A raylib-independent point or vector in continuous coordinates (y grows downwards on screen). Header-only.
struct Float2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Float2() = default;
    constexpr Float2(float px, float py)
        : x(px)
        , y(py) {}

    // Vector at 'radians' clockwise from the x axis (on screen), 'length' long.
    [[nodiscard]] static Float2 FromAngle(float radians, float length = 1.0f) {
        return {std::cos(radians) * length, std::sin(radians) * length};
    }

    friend constexpr bool operator==(Float2, Float2) = default;
    constexpr Float2& operator+=(Float2 other) {
        x += other.x;
        y += other.y;
        return *this;
    }
    constexpr Float2& operator-=(Float2 other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
    constexpr Float2& operator*=(float scale) {
        x *= scale;
        y *= scale;
        return *this;
    }
};

[[nodiscard]] constexpr Float2 operator+(Float2 a, Float2 b) { return {a.x + b.x, a.y + b.y}; }
[[nodiscard]] constexpr Float2 operator-(Float2 a, Float2 b) { return {a.x - b.x, a.y - b.y}; }
[[nodiscard]] constexpr Float2 operator-(Float2 v) { return {-v.x, -v.y}; }
[[nodiscard]] constexpr Float2 operator*(Float2 v, float scale) { return {v.x * scale, v.y * scale}; }
[[nodiscard]] constexpr Float2 operator*(float scale, Float2 v) { return {v.x * scale, v.y * scale}; }
[[nodiscard]] constexpr Float2 operator/(Float2 v, float divisor) { return {v.x / divisor, v.y / divisor}; }

[[nodiscard]] constexpr float Dot(Float2 a, Float2 b) { return a.x * b.x + a.y * b.y; }
// z component of the 3D cross product; positive when 'b' is clockwise from 'a' on screen.
[[nodiscard]] constexpr float Cross(Float2 a, Float2 b) { return a.x * b.y - a.y * b.x; }
[[nodiscard]] constexpr float LengthSquared(Float2 v) { return Dot(v, v); }
[[nodiscard]] inline float Length(Float2 v) { return std::hypot(v.x, v.y); }
[[nodiscard]] inline float Distance(Float2 a, Float2 b) { return Length(b - a); }
// Unit vector; 'fallback' when the length is too small to give a stable direction.
[[nodiscard]] inline Float2 Normalized(Float2 v, Float2 fallback = {}) {
    const float length = Length(v);
    return length > 1e-6f ? v / length : fallback;
}
// Rotated clockwise on screen by 'radians'.
[[nodiscard]] inline Float2 Rotated(Float2 v, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}
// Rotated clockwise on screen by 90 degrees.
[[nodiscard]] constexpr Float2 Perpendicular(Float2 v) { return {-v.y, v.x}; }
// From 'a' to 'b' by 't' (not clamped).
[[nodiscard]] constexpr Float2 Mix(Float2 a, Float2 b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}
