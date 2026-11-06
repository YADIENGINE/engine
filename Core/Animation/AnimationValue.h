#pragma once
#include <cmath>
#include <concepts>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>
#include "Utilities/Reflection.h"

// What can be animated, and how a position is checked. Shared by tracks, clips, timelines and tweens.

// Values with no "between": whole numbers, flags and enums, such as a sprite frame. They hold, then change.
template <class T>
concept HeldValue = std::integral<T> || std::is_enum_v<T>;

// A type with a Mix(a, b, t) of its own beside it, as Float2 and Color4 have.
template <class T>
concept HasMix = requires(const T& a, const T& b, float t) {
    { Mix(a, b, t) } -> std::convertible_to<T>;
};

// What can go between two values: held values, floating point, a type with its own Mix, and a plain struct whose
// fields all can (a pose; a pose inside a pose blends field by field too).
template <class T> [[nodiscard]] consteval bool IsAnimatable() {
    if constexpr (HeldValue<T> || std::floating_point<T> || HasMix<T>) {
        return true;
    } else if constexpr (PlainStruct<T> && std::copyable<T> && std::default_initializable<T>) {
        return []<std::size_t... I>(std::index_sequence<I...>) {
            return (IsAnimatable<FieldType<I, T>>() && ...);
        }(std::make_index_sequence<FieldCount<T>>{});
    } else {
        return false;
    }
}
template <class T>
concept Animatable = IsAnimatable<T>();

template <class T> [[nodiscard]] T MixValue(const T& a, const T& b, float t);
// Every field of a plain struct from a to b by t.
template <PlainStruct T> [[nodiscard]] T MixFields(const T& a, const T& b, float t) {
    T mixed = a;
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        ((FieldAt<I>(mixed) = MixValue(FieldAt<I>(a), FieldAt<I>(b), t)), ...);
    }(std::make_index_sequence<FieldCount<T>>{});
    return mixed;
}
// A value between a and b: held values change at the middle, a type's own Mix wins over field by field.
template <class T> [[nodiscard]] T MixValue(const T& a, const T& b, float t) {
    static_assert(Animatable<T>, "Not animatable: give the type a Mix(a, b, t) or make it a plain struct of animatable "
                                 "fields (Utilities/Reflection.h)");
    if constexpr (HeldValue<T>)
        return t < 0.5f ? a : b;
    else if constexpr (std::floating_point<T>)
        return static_cast<T>(a + (b - a) * static_cast<T>(t));
    else if constexpr (HasMix<T>)
        return Mix(a, b, t);
    else
        return MixFields(a, b, t);
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
