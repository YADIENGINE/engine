#pragma once

// Grid koordinatı. Trivial ve constexpr olduğu için header-only.
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

constexpr Direction Opposite(Direction direction) {
    switch (direction) {
    case Direction::Up   : return Direction::Down;
    case Direction::Down : return Direction::Up;
    case Direction::Left : return Direction::Right;
    case Direction::Right: return Direction::Left;
    }
    return direction;
}

// 'direction'a göre ekranda sola / sağa dönüş (y aşağı doğru artar).
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
