#pragma once
#include <raylib.h>
#include "Utilities/FloatRange.h"
#include "Utilities/Rect.h"

struct SpeedLineStyle {
    int count = 0;
    float rate = 0.0f;
    FloatRange innerRadius; // fraction of half-diagonal
    FloatRange width;       // canvas pixels
    float opacity = 1.0f;
    float whiten = 0.0f;
    float minIntensity = 0.0f;
};

// Stateless; caller chooses clock. Opens its own scissor, so call with no existing scissor scope.
void DrawSpeedLines(const Rect& bounds, float intensity, Color color, float timeSeconds, const SpeedLineStyle& style);
