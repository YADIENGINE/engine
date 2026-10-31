#pragma once
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
#include "Utilities/Color4.h"

// Slow motion: tints the area, darkens the edges and streams slow light trails; fades after 'fadeStart' of its
// duration.
struct SlowMotionStyle {
    float fadeStart = 1.0f;
    float tintAlpha = 0.0f;
    float vignetteSize = 0.0f;
    float vignetteAlpha = 0.0f;
    SpeedLineStyle lines;
};

// All FxStage effect styles, supplied by the theme (Theme::fx) or game. Scenes using individual systems directly
// (previews, showcases) read the same styles here.
struct FxStyle {
    std::size_t particleCapacity = 0;
    std::size_t toastCapacity = 0;
    ParticleRecipeTuning particles;
    CameraEffectsSettings camera;
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
