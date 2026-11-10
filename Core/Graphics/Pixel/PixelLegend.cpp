#include "Graphics/Pixel/PixelLegend.h"
#include <format>
#include <stdexcept>

PixelLegend::PixelLegend() {
    _inks.fill(Unmapped);
    _inks['.'] = Keep;
    _inks[' '] = Keep;
    _inks['0'] = ClearInk;
    for (int digit = 1; digit <= 9; ++digit)
        _inks['0' + digit] = static_cast<std::int16_t>(digit);
    for (int letter = 0; letter < 26; ++letter) {
        _inks['a' + letter] = static_cast<std::int16_t>(10 + letter);
        _inks['A' + letter] = static_cast<std::int16_t>(36 + letter);
    }
}

PixelLegend::PixelLegend(std::initializer_list<std::pair<char, Ink>> letters) {
    _inks.fill(Unmapped);
    _inks['.'] = Keep;
    _inks[' '] = Keep;
    for (const auto& [letter, ink] : letters)
        Set(letter, ink);
}

void PixelLegend::Set(char letter, Ink ink) {
    const auto code = static_cast<unsigned char>(letter);
    if (code < 33 || code > 126 || letter == '.')
        throw std::invalid_argument(std::format("PixelLegend::Set: '{}' cannot be mapped ('.' and ' ' keep)", letter));
    _inks[code] = ink;
}

std::optional<Ink> PixelLegend::Find(char letter) const {
    const auto code = static_cast<unsigned char>(letter);
    const std::int16_t ink = code < _inks.size() ? _inks[code] : Unmapped;
    if (ink == Keep) return std::nullopt;
    if (ink == Unmapped)
        throw std::invalid_argument(std::format("PixelCanvas::Stamp: '{}' is not in the canvas's legend", letter));
    return static_cast<Ink>(ink);
}
