#pragma once
#include <cstddef>
#include "Fx/Announcements.h"
#include "Fx/CameraEffects.h"
#include "Fx/FloatingTexts.h"
#include "Fx/ParticleRecipes.h"
#include "Fx/PulseOverlay.h"
#include "Fx/Rings.h"
#include "Fx/ScreenFlash.h"
#include "Fx/ScreenGlitch.h"
#include "Fx/Shockwaves.h"
#include "Fx/SlowMotionOverlay.h"
#include "Graphics/Draw/SpeedLines.h"
#include "Utilities/Color4.h"

// All FxStage effect styles, supplied by the theme (Theme::fx) or game. Scenes using individual systems directly
// (editor scenes, showcases) read the same styles here.
struct FxStyle {
    std::size_t particleCapacity = 0;
    std::size_t toastCapacity = 0;
    ParticleRecipes particles;
    CameraEffectsStyle camera;
    FloatingTextStyle text;      // FxStage fills maxWidth with its area width
    float textPopSeconds = 0.0f; // PopText entrance
    RingStyle ring;
    FlashStyle flash;
    BannerStyle banner;
    ToastStyle toast;
    SpeedLineStyle speedLines; // Motion (SetMotion)
    PulseOverlayStyle pulse;
    ShockwaveStyle shockwave;
    GlitchStyle glitch;
    SlowMotionStyle slowMotion;
    float vignetteSize = 0.0f; // Persistent viewport edge darkening; 0: none
    Color4 vignetteColor{0, 0, 0, 0};
};
