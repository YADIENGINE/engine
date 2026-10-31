#pragma once
#include "Fx/FxQuality.h"
#include "Graphics/Post/RenderQuality.h"

// The player's graphics choices; Game applies them and writes them to the options file.
struct GraphicsSettings {
    int renderHeight = 0; // Height limit of the canvas texture; 0: the window's resolution
    bool vsync = false;
    bool showFps = false; // Frame rate and frame time in the corner
    RenderQuality render; // Glows, bloom, background animation (the Painter applies it)
    FxQuality fx;         // Particles, shake, motion, flashes (Game::Effects(); the Fx classes apply it)
};
