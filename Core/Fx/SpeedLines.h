#pragma once
#include "Utilities/Color4.h"
#include "Utilities/FloatRange.h"
#include "Utilities/FloatRect.h"

class Painter;

struct SpeedLineStyle {
    int count = 0;
    float rate = 0.0f;
    FloatRange innerRadius; // fraction of half-diagonal
    FloatRange width;       // canvas units
    float opacity = 1.0f;
    float whiten = 0.0f;
    float minIntensity = 0.0f;
};

// Lines rushing in from the edges of 'bounds' (clipped to it). Stateless; the caller chooses the clock.
void DrawSpeedLines(Painter& paint, const FloatRect& bounds, float intensity, Color4 color, float timeSeconds,
                    const SpeedLineStyle& style);
