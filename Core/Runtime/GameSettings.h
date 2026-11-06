#pragma once
#include <filesystem>
#include <vector>
#include "Graphics/Post/PostSettings.h"
#include "Graphics/Render/WindowSettings.h"
#include "Audio/AudioSystem.h"
#include "Audio/SoundTables.h"
#include "Modules/Persistence/Persistence.h"
#include "Graphics/Style/PlainTheme.h"
#include "Ui/Kernel/UiLooks.h"
#include "Ui/Sound/UiSounds.h"
#include "Utilities/Color4.h"

struct GameSettings {
    WindowSettings window;
    int canvasWidth = 1280;
    int canvasHeight = 720;
    Color4 background{0, 0, 0, 255};
    Color4 letterbox{0, 0, 0, 255};
    bool smoothCanvas = true;
    // Full-screen finish (bloom, vignette, grain, tint...); a frame may change it through Painter::Post. The canvas keeps a
    // bloom layer only while bloom is asked for (here, or by the previous frame).
    PostSettings post;
    std::filesystem::path applicationId = "Game";
    // The font Painter draws with when given none (relative to the assets). List it in `assets` too.
    std::filesystem::path font;
    // Fonts for characters `font` lacks, first match wins: another script (Arabic, Hebrew, Thai...) or symbols. List
    // them in `assets` too. See FontFace::AddFallback.
    std::vector<std::filesystem::path> fontFallbacks;
    std::vector<std::filesystem::path> assets;
    AudioCatalog audio;
    // The game's typed sound tables, played through Game::Sounds(); see Docs/Audio.md. `audio` keeps music and any
    // effect a game prefers to number itself.
    SoundTables sounds;
    AudioVolumeSettings audioVolume;
    SaveSettings save;
    // Colours, backgrounds and effects; activated before the game's scenes and views are created (see Style/Theme.h).
    Theme theme = PlainTheme;
    // How widgets look; what a new Ui starts with. Empty: each widget's plain look. See Ui/Kernel/UiLooks.h.
    UiLooks uiLooks;
    // What every Ui plays, from the rows of `sounds`; empty fields are silent. A key that cannot play throws at startup.
    // See Docs/Ui.md.
    UiSounds uiSounds;
    // Transient tools/offscreen runs can use their supplied settings without reading or writing preferences.
    bool persistentOptions = true;
    // Embedded/remote application hosts own the OS window and frame transport dimensions.
    bool fixedWindow = false;
};
