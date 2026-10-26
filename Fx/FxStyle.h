#pragma once
#include <raylib.h>
#include <cstddef>
#include "Fx/CameraEffects2D.h"
#include "Fx/FloatingText2D.h"
#include "Fx/GlitchEffects2D.h"
#include "Fx/OverlayEffects2D.h"
#include "Fx/ParticleRecipes.h"
#include "Fx/PulseOverlay2D.h"
#include "Fx/RingEffects2D.h"
#include "Fx/ShockwaveEffects2D.h"
#include "Fx/SpeedLines.h"

// Ağır çekim: alan rengine çalar, kenarlar kararır, ağır ışık izleri akar; süresinin 'fadeStart' oranından sonra söner.
struct SlowMotionStyle {
    float fadeStart = 1.0f;
    float tintAlpha = 0.0f;
    int vignetteSize = 0;
    float vignetteAlpha = 0.0f;
    SpeedLineStyle lines;
};

// FxStage'in bütün efekt stilleri; tema (Theme::fx) ya da oyun verir. Ayrı sistemleri doğrudan kullanan sahneler
// (önizleme, tanıtım) de aynı stilleri buradan okur.
struct FxStyle {
    std::size_t particleCapacity = 0;
    std::size_t toastCapacity = 0;
    ParticleRecipeTuning particles;
    CameraEffectsSettings camera;
    FloatingTextStyle text;      // maxWidth'i FxStage alan genişliğiyle doldurur
    float textPopSeconds = 0.0f; // PopText girişi
    RingStyle ring;
    FlashStyle flash;
    BannerStyle banner;
    ToastStyle toast;
    SpeedLineStyle speedLines; // Hızlanma (SetMotion)
    PulseOverlayStyle pulse;
    ShockwaveStyle shockwave;
    GlitchStyle glitch;
    SlowMotionStyle slowMotion;
    int vignetteSize = 0; // Görüş alanının kalıcı kenar karartması; 0: yok
    Color vignetteColor = BLANK;
};
