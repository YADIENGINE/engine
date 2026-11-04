#pragma once
#include "Utilities/Geometry/Vec2.h"

enum class Direction { Up, Down, Left, Right };

[[nodiscard]] constexpr Vec2 ToVec2(Direction direction) {
    switch (direction) {
    case Direction::Up   : return {0, -1};
    case Direction::Down : return {0, 1};
    case Direction::Left : return {-1, 0};
    case Direction::Right: return {1, 0};
    }
    return {};
}

[[nodiscard]] constexpr Float2 ToFloat2(Direction direction) { return ToFloat2(ToVec2(direction)); }

[[nodiscard]] constexpr Direction Opposite(Direction direction) {
    switch (direction) {
    case Direction::Up   : return Direction::Down;
    case Direction::Down : return Direction::Up;
    case Direction::Left : return Direction::Right;
    case Direction::Right: return Direction::Left;
    }
    return direction;
}

// Screen-space left / right turn relative to 'direction' (y increases downward).
[[nodiscard]] constexpr Direction TurnLeft(Direction direction) {
    switch (direction) {
    case Direction::Up   : return Direction::Left;
    case Direction::Left : return Direction::Down;
    case Direction::Down : return Direction::Right;
    case Direction::Right: return Direction::Up;
    }
    return direction;
}

[[nodiscard]] constexpr Direction TurnRight(Direction direction) { return Opposite(TurnLeft(direction)); }
