#pragma once
#include <array>
#include <optional>
#include "Graphics/Backgrounds/Sky.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

// How a city looks on its own; every colour left empty keeps the kind's own value.
struct SkylineLook {
    bool sky = true;                                // False: only the city, to lay over another drawing
    std::optional<std::array<Color4, 3>> buildings; // From the farthest row to the nearest
    std::optional<Color4> window;                   // Lit windows
    std::optional<Color4> windowAlt;                // Some windows use this one
    std::optional<Color4> haze;                     // Mist in front of each row
    float scroll = 0.0f;                            // How far the view has moved, in box heights at the nearest row
    float speed = 0.0f;                             // The same, per second of 'time'
    unsigned seed = 1;
};

// Three rows of buildings fading into the distance, whose windows slowly change. Far rows move slower when scrolled.
// Fills 'box' and stays inside it (the current transform must not rotate). 'time' is in seconds.
void DrawSkyline(Painter& paint, const FloatRect& box, SkyKind kind, float time, const SkylineLook& look = {});
