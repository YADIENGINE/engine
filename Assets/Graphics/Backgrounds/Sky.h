#pragma once
#include <optional>
#include <span>
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

// The time of day of a sky. The scenery in this folder (DrawClouds, DrawMountains, DrawSea, DrawSkyline) takes the same
// kind, so its colours match the sky behind it.
enum class SkyKind { Day, Sunset, Night };

// How a sky looks on its own; every field left empty keeps the kind's own value.
struct SkyLook {
    std::optional<Color4> top; // The gradient, from the top of the box to its horizon
    std::optional<Color4> upper;
    std::optional<Color4> lower;
    std::optional<Color4> horizon;
    std::optional<Color4> body;    // The sun or the moon
    std::optional<Float2> bodyAt;  // Its centre, as a share of the box (0..1)
    std::optional<float> bodySize; // Its radius, as a share of the box's short side; 0 leaves it out
    std::optional<float> wisps;    // How many thin cloud streaks, as a share of the kind's own number
    std::optional<float> stars;    // Star density
    unsigned seed = 1;
};

// Fills 'box' and stays inside it (the current transform must not rotate). 'time' is in seconds: stars twinkle and
// streaks creep.
void DrawSky(Painter& paint, const FloatRect& box, SkyKind kind, float time, const SkyLook& look = {});

// Top to bottom through 'colors', evenly spaced, without seams.
void DrawGradientBands(Painter& paint, const FloatRect& box, std::span<const Color4> colors);
