#pragma once
#include "Utilities/CameraView.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRect.h"

// Cinematic camera shot: the field moves to 'focus' (a canvas point), zooms by 'zoom', and rotates by 'rotation'
// radians. 'weight' 0..1 blends from the normal camera to this shot; at 0 the shot has no effect.
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
// Blends focus, zoom, rotation and weight between two shots by 't' (0..1).
[[nodiscard]] CameraShot BlendShots(const CameraShot& from, const CameraShot& to, float t);
// Base cinematic camera; camera effects (CameraEffects2D) are applied on top.
[[nodiscard]] CameraView ShotCamera(const CameraShot& shot, const FloatRect& viewport, const FloatRect& arena);
