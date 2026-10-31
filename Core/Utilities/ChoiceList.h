#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include "Utilities/MathUtils.h"

// Selectable value lists for settings screens (scheme, FPS limit, language ...): reading saved values and stepping.
// Header-only.

// Option matching the raw saved value; 'fallback' if absent from the list.
template <class T, std::size_t N>
constexpr T StoredChoice(std::int64_t stored, const std::array<T, N>& choices, T fallback) {
    for (const T choice : choices)
        if (static_cast<std::int64_t>(choice) == stored) return choice;
    return fallback;
}

// Moves 'steps' options from 'current' (negative: backward), wrapping at both ends; starts at the first if absent.
template <class T, std::size_t N> constexpr T StepChoice(T current, const std::array<T, N>& choices, int steps) {
    const auto found = std::find(choices.begin(), choices.end(), current);
    const int index = found == choices.end() ? 0 : static_cast<int>(found - choices.begin());
    return choices[static_cast<std::size_t>(WrapIndex(index + steps, static_cast<int>(N)))];
}
// For lists filtered at runtime (e.g. window sizes fitting the monitor); returns 'current' for an empty list.
template <class T> constexpr T StepChoice(T current, std::span<const T> choices, int steps) {
    if (choices.empty()) return current;
    const auto found = std::find(choices.begin(), choices.end(), current);
    const int index = found == choices.end() ? 0 : static_cast<int>(found - choices.begin());
    return choices[static_cast<std::size_t>(WrapIndex(index + steps, static_cast<int>(choices.size())))];
}
