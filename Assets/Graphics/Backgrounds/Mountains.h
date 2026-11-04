#pragma once
#include <array>
#include <optional>
#include "Graphics/Backgrounds/Sky.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

// How mountains look on their own; every colour left empty keeps the kind's own value.
struct MountainsLook {
    bool sky = true;                            // False: only the ridges, to lay over another drawing
    std::optional<std::array<Color4, 4>> ridges; // From the farthest ridge to the nearest
    std::optional<Color4> snow;
    std::optional<Color4> haze; // Mist pooling in front of each ridge
    float scroll = 0.0f;        // How far the view has moved, in box heights at the nearest ridge; for a camera
    float speed = 0.0f;         // The same, per second of 'time'
    unsigned seed = 1;
};

// Four ridges fading into the distance, snow on the far peaks and pines on the nearest. Far ridges move slower when
// scrolled. Fills 'box' and stays inside it (the current transform must not rotate).
//
// Still mountains need redrawing only when they change: draw them once into a Surface the caller owns and redraw on
// Painter::Stale.
void DrawMountains(Painter& paint, const FloatRect& box, SkyKind kind, float time, const MountainsLook& look = {});
