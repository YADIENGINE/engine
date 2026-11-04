#pragma once
#include "Utilities/Geometry/Float2.h"

// Something a camera keeps in view. With several, 'weight' pulls the camera towards one (0 ignores it), and 'radius' is
// how much room around the point must stay visible when the camera zooms to fit them (Cinemachine's target group).
struct CameraTarget {
    Float2 position{};
    float weight = 1.0f;
    float radius = 0.0f;
};
