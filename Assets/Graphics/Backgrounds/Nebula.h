#pragma once
#include <optional>
#include "Utilities/Color4.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

enum class NebulaKind { Violet, Teal, Ember };

// How a nebula looks on its own; every colour left empty keeps the kind's own value.
struct NebulaLook {
    std::optional<Color4> space; // From the empty dark to the brightest gas
    std::optional<Color4> shade;
    std::optional<Color4> gas;
    std::optional<Color4> bright;
    std::optional<Color4> accent; // A second gas mixed through the first
    float speed = 1.0f;           // How fast the gas shifts; 0 keeps it still
    float stars = 0.6f;           // Star density; 0 leaves them out
    unsigned seed = 1;
};

// Fills 'box' and stays inside it (the current transform must not rotate). 'time' is in seconds.
//
// The gas is computed per frame. To keep it cheap, draw it once into a Surface the caller owns and redraw on Stale:
//     if (paint.Stale(_layer)) {
//         auto into = paint.Target(_layer);
//         const Float2 size = _layer.Size();
//         DrawNebula(paint, {0.0f, 0.0f, size.x, size.y}, kind, 0.0f, {.speed = 0.0f, .stars = 0.0f});
//     }
//     paint.Image(_layer, box);
//     DrawStarField(paint, box, StarFieldKind::Still, time, {.background = false});
void DrawNebula(Painter& paint, const FloatRect& box, NebulaKind kind, float time, const NebulaLook& look = {});
