#pragma once
#include <optional>
#include "Utilities/Color4.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

enum class SynthwaveKind { Magenta, Cyan, Ember };

// How the scene looks on its own; every colour left empty keeps the kind's own value.
struct SynthwaveLook {
    std::optional<Color4> skyTop; // The gradient, from the top of the box to the horizon
    std::optional<Color4> skyMiddle;
    std::optional<Color4> skyHorizon;
    std::optional<Color4> sunTop;
    std::optional<Color4> sunBottom;
    std::optional<Color4> ridge;  // The dark mountains on the horizon
    std::optional<Color4> rim;    // Their lit edge
    std::optional<Color4> ground;
    std::optional<Color4> grid;
    float horizon = 0.58f; // Where the ground starts, as a share of the box height
    float speed = 1.0f;    // How fast the grid rushes past; 0 keeps it still
    unsigned seed = 1;
};

// A striped sun sinking behind dark ridges over a glowing grid rushing towards the viewer. Fills 'box' and stays inside
// it (the current transform must not rotate). 'time' is in seconds.
void DrawSynthwaveHorizon(Painter& paint, const FloatRect& box, SynthwaveKind kind, float time,
                          const SynthwaveLook& look = {});
