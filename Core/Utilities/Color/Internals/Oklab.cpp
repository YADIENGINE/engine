#include "Utilities/Color/Oklab.h"
#include <algorithm>
#include <cmath>

namespace {
// Ottosson's reference code, unmodified; inside this namespace so its Lab and RGB stay private to this file.
#include <oklab.h>

float Linear(std::uint8_t channel) {
    const float value = static_cast<float>(channel) / 255.0f;
    return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

std::uint8_t Encoded(float linear) {
    const float value = linear <= 0.0031308f ? linear * 12.92f : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}
} // namespace

OklabColor ToOklab(Color4 color) {
    const Lab lab = linear_srgb_to_oklab({Linear(color.r), Linear(color.g), Linear(color.b)});
    return {lab.L, lab.a, lab.b};
}

Color4 FromOklab(OklabColor color, std::uint8_t alpha) {
    const RGB rgb = oklab_to_linear_srgb({color.l, color.a, color.b});
    return {Encoded(rgb.r), Encoded(rgb.g), Encoded(rgb.b), alpha};
}

float OklabDistance(OklabColor first, OklabColor second) {
    const float l = first.l - second.l;
    const float a = first.a - second.a;
    const float b = first.b - second.b;
    return std::sqrt(l * l + a * a + b * b);
}
