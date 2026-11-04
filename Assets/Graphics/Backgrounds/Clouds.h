#pragma once
#include <optional>
#include "Graphics/Backgrounds/Sky.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

// How clouds look on their own; every colour left empty keeps the kind's own value.
struct CloudsLook {
    bool sky = true; // False: only the clouds, to lay over another drawing
    std::optional<Color4> light; // The lit tops
    std::optional<Color4> body;
    std::optional<Color4> shade; // The undersides
    std::optional<Color4> far;   // Far clouds fade towards this
    float amount = 1.0f;
    float speed = 1.0f;
    unsigned seed = 1;
};

// Puffy clouds in three depths, the nearest biggest and fastest, drifting through and wrapping around. Fills 'box' and
// stays inside it (the current transform must not rotate). 'time' is in seconds.
void DrawClouds(Painter& paint, const FloatRect& box, SkyKind kind, float time, const CloudsLook& look = {});
