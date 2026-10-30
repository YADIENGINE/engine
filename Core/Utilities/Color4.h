#pragma once
#include <cstdint>

// A raylib-independent RGBA colour, 0..255 per channel. Headless code and tuning values use it; drawing converts it
// with ToColor (Graphics/Draw/DrawUtils.h).
struct Color4 {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    friend constexpr bool operator==(const Color4&, const Color4&) = default;
};
