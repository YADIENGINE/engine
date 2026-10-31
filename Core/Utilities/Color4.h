#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// A raylib-independent RGBA colour, 0..255 per channel, straight (not premultiplied) alpha. Headless code, tuning
// values and the Painter API use it.
struct Color4 {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

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
// Colour from hue (degrees, any value; wraps), saturation and value (0..1, clamped).
[[nodiscard]] inline Color4 Hsv(float hue, float saturation, float value, std::uint8_t alpha = 255) {
    const float s = std::clamp(saturation, 0.0f, 1.0f);
    const float v = std::clamp(value, 0.0f, 1.0f);
    const float h = std::isfinite(hue) ? hue - 360.0f * std::floor(hue / 360.0f) : 0.0f;
    const auto channel = [&](float n) {
        const float k = std::fmod(n + h / 60.0f, 6.0f);
        const float ramp = std::clamp(std::min(k, 4.0f - k), 0.0f, 1.0f);
        return static_cast<std::uint8_t>(std::lround(255.0f * (v - v * s * ramp)));
    };
    return {channel(5.0f), channel(3.0f), channel(1.0f), alpha};
}
