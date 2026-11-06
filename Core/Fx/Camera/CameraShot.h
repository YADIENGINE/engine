#pragma once
#include "Animation/Pose.h"
#include "Camera/CameraView.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/FloatRect.h"

// Cinematic camera shot: the field moves to 'focus' (a canvas point), zooms by 'zoom', and rotates by 'rotation'
// radians. 'weight' 0..1 blends from the normal camera to this shot; at 0 the shot has no effect. It is an animation
// pose: SecondsTween<CameraShot> moves between shots, and a clip of it is an authored camera move.
struct CameraShot {
    Float2 focus{};
    float zoom = 1.0f;
    float rotation = 0.0f; // Radians, clockwise
    float weight = 0.0f;
    // true: the visible area stays inside the arena when zoomed in (focus is pulled inward); false: focus may move
    // outside the arena to fully frame an object at its edge.
    bool keepInArena = true;
};

// Framing: intersects 'safeArea' (e.g. between cinematic bars) with 'viewport', inset by 'padding' from the edges;
// zoom stays in [minZoom, maxZoom].
struct CameraFraming {
    FloatRect viewport;
    FloatRect safeArea;
    float padding = 0.0f;
    float minZoom = 1.0f;
    float maxZoom = 1.0f;
};

// Shot centering 'bounds' in the safe area (weight 1); empty 'bounds' returns an inactive shot.
[[nodiscard]] CameraShot FrameCameraShot(const FloatRect& bounds, const CameraFraming& framing);
// Base cinematic camera; camera effects (CameraEffects) are applied on top.
[[nodiscard]] CameraView ShotCamera(const CameraShot& shot, const FloatRect& viewport, const FloatRect& arena);
