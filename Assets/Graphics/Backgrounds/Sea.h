#pragma once
#include <optional>
#include "Graphics/Backgrounds/Sky.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

// How the sea looks on its own; every colour left empty keeps the kind's own value.
struct SeaLook {
    bool sky = true; // False: only the water, to lay over another drawing
    std::optional<Color4> far; // The water, from the horizon to the viewer
    std::optional<Color4> middle;
    std::optional<Color4> near;
    std::optional<Color4> crest; // Lit wave tops
    std::optional<Color4> glint; // The sun's or moon's path on the water
    float horizon = 0.46f;       // Where the water starts, as a share of the box height
    float speed = 1.0f;
    unsigned seed = 1;
};

// Rows of waves growing towards the viewer, with the sky's light glittering on them. Fills 'box' and stays inside it
// (the current transform must not rotate). 'time' is in seconds.
void DrawSea(Painter& paint, const FloatRect& box, SkyKind kind, float time, const SeaLook& look = {});
