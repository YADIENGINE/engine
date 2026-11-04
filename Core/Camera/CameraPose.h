#pragma once
#include "Animation/PoseFields.h"
#include "Utilities/Geometry/Float2.h"

// Where a camera is: the world point at the middle of its viewport, its zoom (above 1 shows less, larger) and its
// rotation (radians, clockwise). An animation pose: a SecondsTween<CameraPose> or a clip moves a camera by hand
// (Camera2D::SetPose), and MixPose blends two.
struct CameraPose {
    Float2 center{};
    float zoom = 1.0f;
    float rotation = 0.0f;

    static constexpr auto Fields() {
        return PoseFields(PoseField("center", &CameraPose::center), PoseField("zoom", &CameraPose::zoom),
                          PoseField("rotation", &CameraPose::rotation));
    }
    friend constexpr bool operator==(const CameraPose&, const CameraPose&) = default;
};
