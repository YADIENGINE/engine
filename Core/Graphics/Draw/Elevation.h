#pragma once
#include "Graphics/Paint/Painter.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"

// Height in a 2.5D world: something stands at a point on the ground and is drawn 'height' up the screen from it (a
// jump, a flyer, a thrown pot); its shadow stays on that point, shrinking and fading as it rises. Sort such things by
// their ground point (DepthOrder), not by where they show. Draw the shadows with the floor (StageView::DrawFloor), so
// that none falls over a character, then each thing raised in the world pass:
//
//     DrawGroundShadow(paint, hero.ground, hero.height, {.radius = 20.0f});      // DrawFloor
//     { auto lifted = Raised(paint, hero.height); DrawHero(paint, hero.ground); } // DrawWorld

// How a shadow on the ground looks.
struct GroundShadow {
    float radius = 16.0f;      // Half its width while what casts it stands on the ground
    float flatten = 0.4f;      // Its height against its width: a shadow lies flat in a three-quarter view
    Color4 color{0, 0, 0, 90}; // Alpha is its strength on the ground
    float fadeHeight = 240.0f; // At this height it is gone; on the way up it shrinks to half
};

// The shadow of something 'height' world units above 'ground'. A negative height or an invalid look is rejected the
// way Painter rejects (Painter::Reject).
void DrawGroundShadow(Painter& paint, Float2 ground, float height, const GroundShadow& shadow = {});
// What follows is drawn 'height' world units above where it stands: moved up by it, in the coordinates of the scope
// it opens in (the world, inside paint.Camera).
[[nodiscard]] PaintScope Raised(Painter& paint, float height);
