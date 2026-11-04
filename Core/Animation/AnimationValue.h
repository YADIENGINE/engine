#pragma once
#include <cmath>
#include <concepts>
#include <limits>
#include <type_traits>

// What can be animated, and how a position is checked. Shared by tracks, clips, timelines and tweens.

// Values with no "between": whole numbers, flags and enums, such as a sprite frame. They hold, then change.
template <class T>
concept HeldValue = std::integral<T> || std::is_enum_v<T>;

// A value between a and b. Other types need a Mix(a, b, t) of their own beside them, as Float2 and Color4 have.
template <class T> [[nodiscard]] T MixValue(const T& a, const T& b, float t) {
    if constexpr (HeldValue<T>)
        return t < 0.5f ? a : b;
    else if constexpr (std::floating_point<T>)
        return static_cast<T>(a + (b - a) * static_cast<T>(t));
    else
        return Mix(a, b, t);
}

// Not negative and not infinite. Written this way round so that a NaN position fails too.
template <class Position> [[nodiscard]] constexpr bool ValidAnimationPosition(Position at) {
    return at >= Position{} && at <= std::numeric_limits<Position>::max();
}

// A position mapped into one cycle, 0 <= result < length. A length of 0 has only position 0.
template <class Position> [[nodiscard]] Position WrapAnimationPosition(Position position, Position length) {
    if (length <= Position{}) return Position{};
    Position wrapped{};
    if constexpr (std::integral<Position>)
        wrapped = position % length;
    else
        wrapped = std::fmod(position, length);
    return wrapped < Position{} ? wrapped + length : wrapped;
}
