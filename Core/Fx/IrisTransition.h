#pragma once
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRect.h"

class Painter;

struct IrisStyle {
    Color4 curtain{};
    Color4 innerRim{};
    Color4 outerRim{};
    Color4 glow{}; // Its alpha is the glow's strength
    float rimWidth = 0.0f;
    float glowWidth = 0.0f;
};

// A closing circle around 'focus'; 'cover' 0: open, 1: closed. Clipped to 'bounds'.
void DrawIrisTransition(Painter& paint, const FloatRect& bounds, Float2 focus, float cover, const IrisStyle& style);
