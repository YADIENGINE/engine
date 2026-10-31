#pragma once
#include "Utilities/Float2.h"

// Grid coordinate. Header-only because it is trivial and constexpr.
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

enum class Direction { Up, Down, Left, Right };

constexpr Vec2 ToVec2(Direction direction) {
    switch (direction) {
    case Direction::Up   : return {0, -1};
    case Direction::Down : return {0, 1};
    case Direction::Left : return {-1, 0};
    case Direction::Right: return {1, 0};
    }
    return {};
}

// The same point / unit direction in continuous coordinates (for drawing).
[[nodiscard]] constexpr Float2 ToFloat2(Vec2 v) { return {static_cast<float>(v.x), static_cast<float>(v.y)}; }
[[nodiscard]] constexpr Float2 ToFloat2(Direction direction) { return ToFloat2(ToVec2(direction)); }

constexpr Direction Opposite(Direction direction) {
    switch (direction) {
    case Direction::Up   : return Direction::Down;
    case Direction::Down : return Direction::Up;
    case Direction::Left : return Direction::Right;
    case Direction::Right: return Direction::Left;
    }
    return direction;
}

// Screen-space left / right turn relative to 'direction' (y increases downward).
constexpr Direction TurnLeft(Direction direction) {
    switch (direction) {
    case Direction::Up   : return Direction::Left;
    case Direction::Left : return Direction::Down;
    case Direction::Down : return Direction::Right;
    case Direction::Right: return Direction::Up;
    }
    return direction;
}
constexpr Direction TurnRight(Direction direction) { return Opposite(TurnLeft(direction)); }
