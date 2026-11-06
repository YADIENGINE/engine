#pragma once
#include <filesystem>
#include <vector>
#include "Graphics/Post/PostSettings.h"
#include "Audio/AudioSystem.h"
#include "Audio/SoundTables.h"
#include "Modules/Persistence/Persistence.h"
#include "Graphics/Style/PlainTheme.h"
#include "Runtime/ProjectSettingsStore.h"
#include "Ui/Kernel/UiLooks.h"
#include "Ui/Sound/UiSounds.h"

// What a game starts with. Title, window, canvas, font and identity come from the project's Game/Game.settings.json
// (`project`, Docs/ProjectSettings.md); the rest is code.
struct GameSettings {
    // Edit Game/Game.settings.json (in the editor: Project > Project settings), not these: the file is their only
    // source. Engine hosts (the editor's worker, tools, tests) change them here.
    ProjectSettings project = CurrentProjectSettings();
    // Host flags: an offscreen host keeps the window hidden and running while unfocused.
    bool windowVisible = true;
    bool alwaysRun = false;
    // Full-screen finish (bloom, vignette, grain, tint...); a frame may change it through Painter::Post. The canvas keeps a
    // bloom layer only while bloom is asked for (here, or by the previous frame).
    PostSettings post;
    // Fonts for characters `project.font` lacks, first match wins: another script (Arabic, Hebrew, Thai...) or
    // symbols; relative to the assets, which ship them by themselves. See FontFace::AddFallback.
    std::vector<std::filesystem::path> fontFallbacks;
    // Asset files the game loads itself (relative to the assets); the font, fallbacks and sounds are added for it.
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
