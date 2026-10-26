#pragma once
#include <raylib.h>
#include <filesystem>
#include <vector>
#include "Graphics/Render/WindowSettings.h"
#include "Audio/AudioSystem.hpp"
#include "Modules/Persistence/Persistence.hpp"
#include "Graphics/Style/NeonArcadeTheme.h"

struct GameSettings {
    WindowSettings window;
    int canvasWidth = 1280;
    int canvasHeight = 720;
    Color background = BLACK;
    Color letterbox = BLACK;
    TextureFilter canvasTextureFilter = TEXTURE_FILTER_BILINEAR;
    std::filesystem::path applicationId = "Game";
    std::filesystem::path font;
    // Açılışta yüklenen atlaslar (ekrandaki piksel boyu); yazı kendinden büyük en küçük atlastan çizilir, komşu boyut
    // oranı ~1.25 kalmalı. En küçüğünden küçük yazılar ilk kullanımda tam piksel boyunda rasterize edilir.
    std::vector<int> fontAtlasSizes{40, 48, 56, 64, 80, 96, 120, 144};
    TextureFilter fontTextureFilter = TEXTURE_FILTER_BILINEAR;
    std::vector<int> fontCodepoints;
    std::vector<std::filesystem::path> assets;
    AudioCatalog audio;
    AudioVolumeSettings audioVolume;
    SaveSettings save;
    // Görsel kimlik; oyunun sahneleri ve View'ları kurulmadan önce etkinleşir (bkz. Style/Theme.h).
    Theme theme = NeonArcadeTheme;
    // Transient tools/offscreen runs can use their supplied settings without reading or writing preferences.
    bool persistentOptions = true;
    // Embedded/remote application hosts own the OS window and frame transport dimensions.
    bool fixedWindow = false;
};
