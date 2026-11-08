#pragma once
#include <cstdint>
#include <string_view>
#include "Utilities/Color4.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;
struct LoadingScreenFrame;

// Uppercase A-Z and space, drawn as merged rectangles of a 5x7 pixel alphabet. Position is the top-left corner.
void DrawLoadingPixelText(Painter& paint, std::string_view text, Float2 position, float pixel, Color4 color,
                          float spacing = 1.0f);
[[nodiscard]] float LoadingPixelTextWidth(std::string_view text, float pixel, float spacing = 1.0f);
// Largest centred 16:9 picture inside the canvas; callers fill the surrounding area with their background colour.
[[nodiscard]] FloatRect LoadingScreenArea(Vec2 canvas);
[[nodiscard]] std::uint32_t LoadingScreenHash(std::uint32_t seed);

// A stand-in picture: a coloured field and a sliding block, until a screen has its own drawing.
void DrawLoadingStandIn(Painter& paint, const LoadingScreenFrame& frame, Color4 background, Color4 mark);
