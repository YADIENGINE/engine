#pragma once
#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdint>

struct Color4;

// Migration only (removed with the raylib bridge): a byte RGBA struct other than Color4, i.e. raylib's Color.
template <class T>
concept LegacyRgba = !std::same_as<T, Color4> && requires(T color) {
    requires std::same_as<decltype(color.r), unsigned char>;
    requires std::same_as<decltype(color.g), unsigned char>;
    requires std::same_as<decltype(color.b), unsigned char>;
    requires std::same_as<decltype(color.a), unsigned char>;
};

// A raylib-independent RGBA colour, 0..255 per channel, straight (not premultiplied) alpha. Headless code, tuning
// values and the Painter API use it.
struct Color4 {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    // Migration only: converts to raylib's Color so code not yet on Painter can use Color4 constants. One way on
    // purpose (raylib colours convert with ToColor4), so 'condition ? Color4 : Color' stays unambiguous.
    template <LegacyRgba T> constexpr operator T() const { return T{r, g, b, a}; }

    friend constexpr bool operator==(const Color4&, const Color4&) = default;
};

// 0xRRGGBB with the given alpha.
[[nodiscard]] constexpr Color4 Rgb(std::uint32_t rgb, std::uint8_t alpha = 255) {
    return {static_cast<std::uint8_t>((rgb >> 16) & 0xFFU), static_cast<std::uint8_t>((rgb >> 8) & 0xFFU),
            static_cast<std::uint8_t>(rgb & 0xFFU), alpha};
}
// Replaces the alpha with 'alpha' (0..1, clamped); for opaque palette colours.
[[nodiscard]] inline Color4 WithAlpha(Color4 color, float alpha) {
    color.a = static_cast<std::uint8_t>(std::lround(255.0f * std::clamp(alpha, 0.0f, 1.0f)));
    return color;
}
// Multiplies the colour's own alpha by 'factor' (clamped to 0..1); for colours that may already be translucent.
[[nodiscard]] inline Color4 ScaleAlpha(Color4 color, float factor) {
    color.a = static_cast<std::uint8_t>(std::lround(static_cast<float>(color.a) * std::clamp(factor, 0.0f, 1.0f)));
    return color;
}
// Channel-wise blend from 'from' to 'to' by 't' (clamped to 0..1), alpha included.
[[nodiscard]] inline Color4 Mix(Color4 from, Color4 to, float t) {
    const float k = std::clamp(t, 0.0f, 1.0f);
    const auto channel = [k](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(
            std::lround(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * k));
    };
    return {channel(from.r, to.r), channel(from.g, to.g), channel(from.b, to.b), channel(from.a, to.a)};
}
