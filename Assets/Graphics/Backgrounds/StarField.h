#pragma once
#include "Utilities/Color4.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

// Still: stars twinkling in place. Drift: three layers sliding sideways at their own speeds. Warp: streaks rushing out
// of the centre.
enum class StarFieldKind { Still, Drift, Warp };

// How the field looks on its own; a game changes only what it needs.
struct StarFieldLook {
    bool background = true; // False: only the stars, to lay over another drawing
    Color4 top = Rgb(0x04061a);
    Color4 bottom = Rgb(0x10123a);
    Color4 star = Rgb(0xf2f4ff);
    Color4 warm = Rgb(0xffd9a8); // Some stars lean to these two
    Color4 cool = Rgb(0x9cc4ff);
    float density = 1.0f;
    float speed = 1.0f;
    float fade = 0.0f; // 0: even over the box; 1: thinning to none at its bottom; 2: to none at its middle
    unsigned seed = 1;
};

// Fills 'box' and stays inside it (the current transform must not rotate). 'time' is in seconds.
void DrawStarField(Painter& paint, const FloatRect& box, StarFieldKind kind, float time,
                   const StarFieldLook& look = {});
