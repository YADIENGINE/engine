#pragma once
#include <optional>
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/FloatRect.h"

// How a camera follows its targets. Sizes given as a fraction are of the visible area, so they hold at any zoom.
struct CameraFollow {
    // The box around the middle of the view the targets move in without moving the camera (Godot's drag margins,
    // Cinemachine's dead zone): {0.2, 0.3} is a fifth of the width and three tenths of the height. 0 keeps them
    // centred.
    Float2 deadZone{};
    // Seconds the camera takes to catch up, slowing down without overshooting; 0 follows at once.
    float smoothTime = 0.15f;
    // Seconds of the targets' motion to look ahead, per axis: {0.4, 0} for a side-scroller. 0 looks nowhere ahead.
    Float2 lookAhead{};
    // The most look-ahead, as a fraction of the visible area.
    float lookAheadLimit = 0.25f;
    // Seconds look-ahead takes to settle, so a turn swings the view over instead of jerking it.
    float lookAheadSmoothTime = 0.4f;
    // World units added to where the targets are: look a little above a character's feet.
    Float2 offset{};
};

// How a camera zooms. Without 'fit' it keeps 'zoom'; with it, it zooms to keep every target's circle in view with
// 'padding' canvas units to spare, between 'min' (the farthest out) and 'max'.
struct CameraZoom {
    float zoom = 1.0f;
    bool fit = false;
    float min = 0.5f;
    float max = 1.0f;
    float padding = 0.0f;
    float smoothTime = 0.3f;
};

struct CameraSettings {
    CameraFollow follow{};
    CameraZoom zoom{};
    // The world the camera may show (Godot's limits, Cinemachine's confiner). On an axis where the view is larger than
    // the limits, they are centred. None: anywhere.
    std::optional<FloatRect> limits;
    // Moves the world by whole canvas units only: crisp code-drawn lines on a slowly moving camera.
    bool pixelSnap = false;
};

// False for negative or non-finite values, a dead zone or look-ahead limit above 1, zooms that are not above 0, min
// above max, or empty limits.
[[nodiscard]] bool IsValid(const CameraSettings& settings);
