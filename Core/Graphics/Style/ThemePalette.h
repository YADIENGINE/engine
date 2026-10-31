#pragma once
#include "Utilities/Color4.h"

// Theme role colors, used with the same meaning throughout the UI. Game-specific accent colors (objects, tiers,
// categories) belong to the game's own palette.
struct ThemePalette {
    Color4 background{0, 0, 0, 255};         // Screen background
    Color4 surface{0, 0, 0, 255};            // Panel, card, preview body
    Color4 surfaceFrame{130, 130, 130, 255}; // Unselected / locked panel border
    Color4 text{255, 255, 255, 255};         // Primary text
    Color4 textSoft{200, 200, 200, 255};     // Secondary text: readable on dark surfaces
    Color4 textDim{130, 130, 130, 255};      // Dim text, hints
    Color4 overlay{0, 0, 0, 0};              // Screen-dimming curtain
    Color4 accent{255, 255, 255, 255};       // Primary accent (confirmation, progress)
    Color4 warning{230, 41, 55, 255};        // Danger, warning
};
