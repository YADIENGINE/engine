#pragma once
#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <utility>
#include "Graphics/Pixel/PixelPalette.h"

// The characters PixelCanvas::Stamp reads. '.' and ' ' keep what is there; every other character must be mapped.
// The default maps '0' to the transparent ink (erases), '1'..'9' to inks 1..9, 'a'..'z' to 10..35 and 'A'..'Z' to
// 36..61.
class PixelLegend {
public:
    PixelLegend();
    // Only these characters (plus '.' and ' ', which keep).
    PixelLegend(std::initializer_list<std::pair<char, Ink>> letters);

    // Maps a printable ASCII character other than '.' and ' '; invalid_argument otherwise.
    void Set(char letter, Ink ink);
    // The ink a character stamps, nullopt for '.' and ' ' (keep). An unmapped character is invalid_argument.
    [[nodiscard]] std::optional<Ink> Find(char letter) const;

private:
    static constexpr std::int16_t Keep = -1;
    static constexpr std::int16_t Unmapped = -2;
    std::array<std::int16_t, 128> _inks{};
};
