#pragma once
#include "Graphics/Draw/EffectQuality.h"

// Oyuncunun grafik tercihleri; Game uygular ve tercih dosyasına yazar.
struct GraphicsSettings {
    int renderHeight = 0; // Canvas dokusunun yükseklik sınırı; 0: pencere çözünürlüğü
    bool vsync = false;
    bool showFps = false; // Köşede FPS ve kare süresi
    EffectQuality effects;
};
