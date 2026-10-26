#pragma once
#include <raylib.h>
#include "Utilities/Rect.h"

struct IrisStyle {
    Color curtain{};
    Color innerRim{};
    Color outerRim{};
    Color glow{};
    float rimWidth = 0.0f;
    float glowWidth = 0.0f;
};

// Canvas koordinatlarında çizer; cover: 0 açık, 1 kapalı. Glow gücü rengin alpha'sıdır.
// Bounds'a kırpar; çağrılırken başka bir scissor scope'u etkin olmamalıdır.
void DrawIrisTransition(const Rect& bounds, Vector2 focus, float cover, const IrisStyle& style);
