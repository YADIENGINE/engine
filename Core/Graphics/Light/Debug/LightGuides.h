#pragma once
#include "Camera/CameraView.h"
#include "Utilities/Color4.h"

class Painter;
struct LightFrame;

struct LightGuidesStyle {
    float width = 1.5f;
    Color4 reach{255, 210, 90, 255};
    Color4 core{255, 255, 255, 200};
};

// Shows the lights of a frame over a scene: where each stands (a cross), how far it reaches, its core and its cone, so
// that what is lit can be explained at a glance. Draw it after the world and outside paint.Camera, with the view the
// lights were given. For views only: a simulation cannot include this header.
void DrawLightGuides(Painter& paint, const LightFrame& frame, const CameraView& view = {},
                     const LightGuidesStyle& style = {});
