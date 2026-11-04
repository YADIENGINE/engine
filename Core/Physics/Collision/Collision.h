#pragma once
#include <optional>
#include "Physics/Collision/Pose.h"
#include "Physics/Collision/Shape.h"
#include "Utilities/Geometry/Float2.h"

// Questions about two shapes, with nothing to set up. For many objects use CollisionWorld. Touching counts as
// overlapping. Results are the same in every run of one build; Docs/Collision.md has the guarantee.

// How two overlapping shapes meet. Moving B by normal * depth separates them.
struct CollisionContact {
    Float2 normal; // Unit vector from A towards B
    float depth = 0.0f;
    Float2 point; // The deepest point of contact
};

// Where a moving shape or a ray first touches something.
struct CastHit {
    Float2 point;
    Float2 normal;         // Faces the mover; zero when it started inside (fraction 0)
    float fraction = 0.0f; // 0..1 along the movement; the mover stops a hair before the surface
};

[[nodiscard]] bool ShapesOverlap(const Shape& a, const Pose& poseA, const Shape& b, const Pose& poseB);
[[nodiscard]] bool ShapeContains(const Shape& shape, const Pose& pose, Float2 point);
// The gap between the surfaces; 0 when they overlap.
[[nodiscard]] float ShapeDistance(const Shape& a, const Pose& poseA, const Shape& b, const Pose& poseB);
// Empty when they do not overlap.
[[nodiscard]] std::optional<CollisionContact> ShapeContact(const Shape& a, const Pose& poseA, const Shape& b,
                                                           const Pose& poseB);
// A ray from 'from' to 'to' against one shape.
[[nodiscard]] std::optional<CastHit> CastRay(Float2 from, Float2 to, const Shape& target, const Pose& pose);
// 'moving' travels by 'move' without turning. It cannot pass through 'target', however fast it goes.
[[nodiscard]] std::optional<CastHit> CastShape(const Shape& moving, const Pose& from, Float2 move, const Shape& target,
                                               const Pose& pose);
