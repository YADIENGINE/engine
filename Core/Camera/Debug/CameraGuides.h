#pragma once
#include "Utilities/Color4.h"

class Camera2D;
class Painter;

struct CameraGuidesStyle {
    float width = 1.5f;
    Color4 viewport{255, 255, 255, 140};
    Color4 deadZone{90, 220, 255, 255};
    Color4 limits{255, 120, 150, 255};
    Color4 targets{130, 240, 140, 255};
    Color4 focus{255, 210, 90, 255};
};

// Shows how a camera follows, over a scene: its viewport, the dead zone around where it is heading, its limits, each
// target's circle, and the point it aims at with look-ahead (a cross) next to where it is heading (a dot). Draw it in
// canvas coordinates, after the world and outside paint.Camera. For views only: a simulation cannot include this
// header.
void DrawCameraGuides(Painter& paint, const Camera2D& camera, const CameraGuidesStyle& style = {});
