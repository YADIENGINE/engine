#include "Graphics/Pixel/PixelPalette.h"
#include <algorithm>
#include <atomic>
#include <format>
#include <stdexcept>

namespace {
std::uint64_t NextRevision() {
    static std::atomic<std::uint64_t> next{1};
    return next.fetch_add(1, std::memory_order_relaxed);
}
} // namespace

PixelPalette::PixelPalette()
    : _colors{Color4{0, 0, 0, 0}}
    , _revision(NextRevision()) {}

PixelPalette::PixelPalette(std::initializer_list<Color4> colors)
    : PixelPalette(std::span<const Color4>(colors.begin(), colors.size())) {}

PixelPalette::PixelPalette(std::span<const Color4> colors)
    : PixelPalette() {
    if (colors.size() >= static_cast<std::size_t>(MaxInks))
        throw std::invalid_argument(std::format("PixelPalette: {} colours; a palette holds at most 255", colors.size()));
    _colors.insert(_colors.end(), colors.begin(), colors.end());
}

void PixelPalette::Touch() { _revision = NextRevision(); }

Ink PixelPalette::Add(Color4 color) {
    if (_colors.size() >= static_cast<std::size_t>(MaxInks))
        throw std::invalid_argument("PixelPalette::Add: all 255 inks are taken");
    _colors.push_back(color);
    Touch();
    return static_cast<Ink>(_colors.size() - 1);
}

void PixelPalette::Set(Ink ink, Color4 color) {
    if (ink == ClearInk || ink >= _colors.size())
        throw std::invalid_argument(
            std::format("PixelPalette::Set: ink {} is not in the palette (inks 1..{})", ink, _colors.size() - 1));
    _colors[ink] = color;
    Touch();
}

Color4 PixelPalette::operator[](Ink ink) const { return ink < _colors.size() ? _colors[ink] : Color4{0, 0, 0, 0}; }

PixelPalette PixelPalette::Mixed(Color4 color, float amount) const {
    PixelPalette mixed = *this;
    if (!(amount > 0.0f)) return mixed;
    for (std::size_t ink = 1; ink < mixed._colors.size(); ++ink) {
        const Color4 before = mixed._colors[ink];
        Color4 after = Mix(before, color, std::min(amount, 1.0f));
        after.a = before.a;
        mixed._colors[ink] = after;
    }
    mixed.Touch();
    return mixed;
}
