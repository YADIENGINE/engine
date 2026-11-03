#pragma once
#include "Physics/Collision/CollisionMask.h"

class CollisionWorld;
class Painter;

struct ColliderDrawStyle {
    float width = 1.5f;
    CollisionMask layers = CollisionMask::All();
};

// Outlines the colliders on top of a scene, one color per layer, so that what the rules see can be compared with what
// the view draws. For views only: a simulation cannot include this header.
void DrawColliders(Painter& paint, const CollisionWorld& world, const ColliderDrawStyle& style = {});
