#pragma once
#include "Physics/Collision/CollisionMask.h"

class CollisionWorld;
class LightOccluders;

// Adds the colliders in 'mask' to 'into', for walls whose light should stop where a player stops. A one-way bridge:
// light does not reach collision, and it does not change the state a replay checks. Use it for what stands still and
// call it again when the level changes. What moves is drawn between simulation steps, so give it to LightOccluders with
// the position the view draws, or its shadow lags behind it. Do not add a collision layer only for light: layers are
// part of a simulation's state. Curved shapes become polygons. For views only: a simulation cannot include this header.
void AddOccluders(LightOccluders& into, const CollisionWorld& world, CollisionMask mask, bool selfLit = true);
