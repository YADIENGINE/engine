#pragma once
#include "Runtime/GraphicsSettings.h"

// What the options screen shows and changes. Plain values, so the screen also runs without a game; a running game
// gives and takes them with Game::UiOptions and Game::ApplyUiOptions.
struct UiOptionsValues {
    int musicPercent = 100;
    int effectsPercent = 100;
    bool fullscreen = false;
    int fpsLimit = 60; // 0: unlimited
    GraphicsSettings graphics{};
};
