#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include "Utilities/MathUtils.h"

// Ayar ekranlarının seçilebilir değer listeleri (şema, FPS sınırı, dil ...): kayıttan okuma ve ileri / geri adım.
// Header-only.

// Kayıttaki ham değere eşit seçenek; listede yoksa 'fallback'.
template <class T, std::size_t N>
constexpr T StoredChoice(std::int64_t stored, const std::array<T, N>& choices, T fallback) {
    for (const T choice : choices)
        if (static_cast<std::int64_t>(choice) == stored) return choice;
    return fallback;
}

// 'current'tan 'steps' seçenek ileri (eksi geri), başa / sona sarar; 'current' listede yoksa ilk seçenekten sayılır.
template <class T, std::size_t N> constexpr T StepChoice(T current, const std::array<T, N>& choices, int steps) {
    const auto found = std::find(choices.begin(), choices.end(), current);
    const int index = found == choices.end() ? 0 : static_cast<int>(found - choices.begin());
    return choices[static_cast<std::size_t>(WrapIndex(index + steps, static_cast<int>(N)))];
}
// Çalışma anında süzülen listeler için (ör. monitöre sığan pencere boyutları); boş listede 'current'.
template <class T> constexpr T StepChoice(T current, std::span<const T> choices, int steps) {
    if (choices.empty()) return current;
    const auto found = std::find(choices.begin(), choices.end(), current);
    const int index = found == choices.end() ? 0 : static_cast<int>(found - choices.begin());
    return choices[static_cast<std::size_t>(WrapIndex(index + steps, static_cast<int>(choices.size())))];
}
