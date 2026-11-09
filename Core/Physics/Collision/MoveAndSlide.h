#pragma once
#include <array>
#include <cstddef>
#include <span>
#include "Physics/Collision/CollisionWorld.h"
#include "Physics/Collision/Pose.h"
#include "Physics/Collision/Shape.h"
#include "Utilities/Geometry/Float2.h"

// Box2D treats shapes this close as touching, so a smaller skin would start the next move stuck to the wall.
inline constexpr float MinSlideSkin = 0.02f;
inline constexpr std::size_t MaxSlideHits = 4;

struct SlideOptions {
    CollisionMask mask;  // What stops the mover
    ColliderId ignore;   // The mover's own collider, when it has one in the world
    float skin = 0.05f;  // Gap kept against what stops it, in world units; at least MinSlideSkin
    // Moving straight into a wall with an opening at most this far to one side moves the mover sideways towards the
    // opening, at most as far as the move: a doorway that needs no exact aim, a head that slips past a ledge's corner.
    // The nearer opening wins. 0 turns it off.
    float cornerAssist = 0.0f;
};

struct SlideResult {
    Float2 position;                              // Where the mover ends; its angle does not change
    std::array<ColliderHit, MaxSlideHits> hits{}; // What it met, in order; each normal faces the mover
    std::size_t hitCount = 0;
    bool blocked = false;  // Something took part of the move away
    bool assisted = false; // cornerAssist moved it sideways
    [[nodiscard]] std::span<const ColliderHit> Hits() const { return {hits.data(), hitCount}; }
};

// Moves a shape that the game moves itself (a character, an enemy, a pushed crate) by 'move' through 'world': it stops
// a skin short of what it meets and slides along it with what is left of the move, also along slanted walls and round
// corners; in a corner it stops. It never passes through a collider, however fast it goes. Only queries: the game
// keeps the position and calls world.Move for the mover's own collider. A mover that starts inside a collider is
// pushed out first. A floor below is a hit whose normal points up: a platformer's "on the ground". The same input
// gives the same result in every run of one build. Docs/Collision.md#moving-and-sliding.
// A skin below MinSlideSkin, a negative cornerAssist or a move that is not finite throws std::invalid_argument.
[[nodiscard]] SlideResult MoveAndSlide(const CollisionWorld& world, const Shape& shape, const Pose& from, Float2 move,
                                       const SlideOptions& options);
