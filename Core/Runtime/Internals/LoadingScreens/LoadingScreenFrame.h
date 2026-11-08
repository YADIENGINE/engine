#pragma once
#include "Utilities/Geometry/Vec2.h"

class Surface;

// What one of the engine's loading screens draws from. Timing, skipping and closing belong to EngineLoadingScene; a
// screen only draws. A game may have no font or files, so a screen uses neither.
struct LoadingScreenFrame {
    Vec2 canvas;
    float seconds = 0.0f;    // Since the screen opened; keeps growing while the sounds load
    float visibility = 0.0f; // 0: the whole picture is black, finish included; 1: fully shown
    Surface& pixels;         // Offscreen picture of LoadingScreenPixelSize with sharp pixels, for a big-pixel screen
};

inline constexpr Vec2 LoadingScreenPixelSize{320, 180};
