#pragma once
#include <raylib.h>
#include <raymath.h>
#include <cmath>
#include "Utilities/Float2.h"
#include "Utilities/Vec2.h"

// Canvas koordinatında (y aşağı) Vector2 aritmetiği. raymath'in C++ operatörleri buradan gelir: a + b, a - b, v * s,
// v / s, a * b (bileşen bileşen), +=, -=, *=, ==; uzun adlı fonksiyonları da (Vector2Lerp, Vector2Reflect ...).
// Aşağıdakiler View'ların sık kullandığı kısa adlar; yerel kopya yazılmaz. Header-only.

[[nodiscard]] inline Vector2 operator-(Vector2 v) { return {-v.x, -v.y}; }

[[nodiscard]] inline float Length(Vector2 v) { return std::hypot(v.x, v.y); }
[[nodiscard]] inline float Distance(Vector2 a, Vector2 b) { return Length(b - a); }
[[nodiscard]] inline float Dot(Vector2 a, Vector2 b) { return a.x * b.x + a.y * b.y; }
// Birim vektör; boyu sıfıra çok yakınsa (yön titremesin diye) 'fallback'.
[[nodiscard]] inline Vector2 Normalized(Vector2 v, Vector2 fallback = {}) {
    const float length = Length(v);
    return length > 1e-4f ? v / length : fallback;
}
// Ekranda saat yönünde 'radians' kadar döndürür.
[[nodiscard]] inline Vector2 Rotated(Vector2 v, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}
// Ekranda saat yönünde 90° döndürülmüş dik vektör.
[[nodiscard]] constexpr Vector2 Perpendicular(Vector2 v) { return {-v.y, v.x}; }
// x ekseninden saat yönünde 'radians' açısında, 'radius' boyunda vektör.
[[nodiscard]] inline Vector2 Polar(float radians, float radius = 1.0f) {
    return {std::cos(radians) * radius, std::sin(radians) * radius};
}

[[nodiscard]] constexpr Vector2 ToVector2(Vec2 v) { return {static_cast<float>(v.x), static_cast<float>(v.y)}; }
[[nodiscard]] constexpr Vector2 ToVector2(Float2 v) { return {v.x, v.y}; }
[[nodiscard]] constexpr Vector2 ToVector2(Direction direction) { return ToVector2(ToVec2(direction)); }
[[nodiscard]] constexpr Float2 ToFloat2(Vector2 v) { return {v.x, v.y}; }
