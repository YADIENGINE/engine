#pragma once
#include <cstddef>
#include "Fx/Camera/CameraEffects.h"
#include "Fx/Particles/ParticleRecipes.h"
#include "Fx/Screen/Announcements.h"
#include "Fx/Screen/PulseOverlay.h"
#include "Fx/Screen/ScreenFlash.h"
#include "Fx/Screen/ScreenGlitch.h"
#include "Fx/Screen/SlowMotionOverlay.h"
#include "Fx/World/FloatingTexts.h"
#include "Fx/World/Rings.h"
#include "Fx/World/Shockwaves.h"
#include "Graphics/Draw/SpeedLines.h"
#include "Utilities/Color4.h"

// All FxStage effect styles, supplied by the theme (Theme::fx) or game. Scenes using individual systems directly
// (catalog scenes, showcases) read the same styles here.
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
