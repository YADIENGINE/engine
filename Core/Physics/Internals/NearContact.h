#pragma once
#include <optional>
#include "Physics/Collision/Pose.h"
#include "Physics/Collision/Shape.h"
#include "Utilities/Geometry/Float2.h"

// How two shapes meet when they overlap or nearly touch. Moving B by normal * (gap - separation) leaves 'gap' between
// them. ShapeContact is the overlapping case; MoveAndSlide also needs the shapes that touch within Box2D's tolerance.
struct NearContactResult {
    Float2 normal;           // Unit vector from A towards B
    float separation = 0.0f; // Negative when they overlap
    Float2 point;
};

// Empty when the shapes are further apart than Box2D's speculative distance (a few hundredths of a unit).
[[nodiscard]] std::optional<NearContactResult> NearContact(const Shape& a, const Pose& poseA, const Shape& b,
                                                           const Pose& poseB);
