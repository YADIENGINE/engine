#pragma once
#include <span>
#include <vector>
#include "Camera/CameraView.h"
#include "Graphics/Paint/PaintStyle.h"
#include "Graphics/Surface/Surface.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Visibility.h"

class LightOccluders;
class Painter;

// What lights a world this frame. Plain data, described every frame, so a split screen gives the same frame to each
// view.
struct LightFrame {
    Color4 ambient{255, 255, 255, 255}; // The light everything gets; white: lights change nothing. Alpha is ignored.
    std::span<const LightSource> lights;
    std::span<const LightOccluders* const> occluders; // What stops the lights; usually a set that stands still and one
                                                      // filled for what moves
};

struct LightLayerOptions {
    float resolution = 0.5f; // Size of the light image against the target, 0.25..1. Lower is cheaper and softer.
};

// Darkens what was drawn so far to 'ambient plus the lights'. Light can only reveal what is there: it never brightens a
// pixel past its own colour. Bloom is not darkened. Draw the world first, this next, then whatever shines by itself
// and the HUD. Light is only what is shown: it decides nothing, and a simulation cannot include this header. A layer
// must not outlive the game's renderer (destroy it with the scene or view that owns it).
class LightLayer {
public:
    explicit LightLayer(const LightLayerOptions& options = {});

    // Call it outside paint.Camera and paint.Transform, and give it the view the world was drawn through, shake
    // included: the lights land on the same places. An empty 'view' suits a game without a camera. A view with a
    // viewport darkens only that part of the target.
    void Draw(Painter& paint, const LightFrame& frame, const CameraView& view = {}) const;

private:
    float _resolution;
    mutable Surface _image; // Holds nothing between frames; only its size and GPU memory are kept
    mutable VisibilityPolygon _visibility;
    mutable std::vector<VisibilityEdge> _blocking;
};
