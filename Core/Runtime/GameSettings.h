#pragma once
#include <filesystem>
#include <vector>
#include "Graphics/Post/PostSettings.h"
#include "Graphics/Render/WindowSettings.h"
#include "Audio/AudioSystem.h"
#include "Modules/Persistence/Persistence.h"
#include "Graphics/Style/NeonArcadeTheme.h"
#include "Utilities/Color4.h"

struct GameSettings {
    WindowSettings window;
    int canvasWidth = 1280;
    int canvasHeight = 720;
    Color4 background{0, 0, 0, 255};
    Color4 letterbox{0, 0, 0, 255};
    bool smoothCanvas = true;
    // Full-screen finish (bloom, vignette, scanlines); a frame may change it through Painter::Post. The canvas keeps a
    // bloom layer only while bloom is asked for (here, or by the previous frame).
    PostSettings post;
    std::filesystem::path applicationId = "Game";
    std::filesystem::path font;
    std::vector<std::filesystem::path> assets;
    AudioCatalog audio;
    AudioVolumeSettings audioVolume;
    SaveSettings save;
    // Visual identity; activated before the game's scenes and views are created (see Style/Theme.h).
    Theme theme = NeonArcadeTheme;
    // Transient tools/offscreen runs can use their supplied settings without reading or writing preferences.
    bool persistentOptions = true;
    // Embedded/remote application hosts own the OS window and frame transport dimensions.
    bool fixedWindow = false;
};
