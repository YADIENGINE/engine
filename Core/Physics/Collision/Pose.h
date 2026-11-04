#pragma once
#include "Utilities/Geometry/Float2.h"

// Where a shape stands: its origin and its angle in radians, clockwise on screen. `{position}` means no rotation.
struct Pose {
    Float2 position{};
    float angle = 0.0f;
    friend constexpr bool operator==(const Pose&, const Pose&) = default;
};
