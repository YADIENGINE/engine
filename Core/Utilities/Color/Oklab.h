#pragma once
#include <cstdint>
#include "Utilities/Color4.h"

// A colour in Oklab, Björn Ottosson's perceptual colour space (ThirdParty/Oklab): `l` is the lightness seen, from 0
// (black) to 1 (white); `a` runs green to red and `b` blue to yellow, both within about -0.4..0.4. Equal distances look
// about equally different, so it answers "how different do these colours look": shading steps, the nearest palette
// colour. Library EngineColor.
struct OklabColor {
    float l = 0.0f;
    float a = 0.0f;
    float b = 0.0f;
};

// An sRGB colour (alpha ignored) in Oklab.
[[nodiscard]] OklabColor ToOklab(Color4 color);
// Back to sRGB, each channel clamped to 0..255.
[[nodiscard]] Color4 FromOklab(OklabColor color, std::uint8_t alpha = 255);
// How far apart two colours look (Euclidean in Oklab); about 0.02 is barely visible side by side.
[[nodiscard]] float OklabDistance(OklabColor first, OklabColor second);
