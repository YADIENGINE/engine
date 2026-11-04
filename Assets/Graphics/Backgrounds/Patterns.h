#pragma once
#include <optional>
#include "Utilities/Color4.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

// Dots: a staggered grid sliding slowly. DotWave: dots swelling in a wave. Hexagons: a honeycomb of outlines sliding
// slowly. HexPulse: a honeycomb whose cells light up in rings.
enum class PatternKind { Dots, DotWave, Hexagons, HexPulse };

// How a pattern looks on its own; every colour left empty keeps the kind's own value.
struct PatternLook {
    bool background = true; // False: only the marks, to lay over another drawing
    std::optional<Color4> top;
    std::optional<Color4> bottom;
    std::optional<Color4> mark;   // The dots or the cell outlines
    std::optional<Color4> bright; // Where a wave or a pulse passes
    float cell = 0.075f;          // Pitch of the grid, as a share of the box's short side
    float speed = 1.0f;
};

// Fills 'box' and stays inside it (the current transform must not rotate). 'time' is in seconds.
void DrawPattern(Painter& paint, const FloatRect& box, PatternKind kind, float time, const PatternLook& look = {});
