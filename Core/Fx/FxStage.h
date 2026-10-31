#pragma once
#include <optional>
#include <string>
#include "Fx/CameraShot.h"
#include "Fx/FxQuality.h"
#include "Fx/FxStyle.h"
#include "Utilities/CameraView.h"
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRect.h"

class Painter;

// Ready-made effects stage for a game field: particles, flying text, rings, shockwaves, camera (shake, punch, kick,
// motion, cinematic shot), flash / banner / toast, pulse, glitch and slow motion. Positions use canvas coordinates;
// styles are supplied at setup (usually CurrentTheme().fx). 'area' is where effects play; 'viewArea' is the viewport
// covered by edge effects (vignette, flash). Lifetimes advance with Update time; shake, speed lines and glitch
// patterns derive from the drawing clock (Painter::Time).
class FxStage {
public:
    FxStage(const FloatRect& area, const FloatRect& viewArea, const FxStyle& style);
    // The player's effect choice (Game::Effects()) for particles, camera, flashes and glitches.
    void SetQuality(const FxQuality& quality);
    void Update(float deltaSeconds);
    void Clear();
    void Burst(Float2 at, Color4 color, int count, ParticleRecipe recipe = ParticleRecipe::Soft);
    void Trail(Float2 at, Color4 color);
    // 'facing': movement direction; sparks scatter behind it.
    void Dash(Float2 at, Float2 facing, Color4 color);
    void Steam(Float2 at, Color4 color);
    void FloatText(Float2 at, const std::string& text, Color4 color);
    // Flying text of 'size' with a popping entrance.
    void PopText(Float2 at, const std::string& text, Color4 color, float size);
    void Ring(Float2 at, Color4 color);
    void Toast(const std::string& text, Color4 color);
    void Flash(Color4 color);
    void Banner(const std::string& text);
    void Shake(float seconds);
    void Punch(float amount, std::optional<Float2> focus = std::nullopt);
    void Kick(Float2 direction);
    // Each frame: motion intensity (0-1); camera shifts forward, speed lines flow in 'color'.
    void SetMotion(float intensity, Color4 color, Float2 facing);
    // Each frame: tints the area toward 'color', edges pulse like a heartbeat (smooth transition).
    void SetPulse(bool active, Color4 color);
    // Double-ring shockwave from 'at'; spans 'reach' units when > 0, otherwise the whole area.
    void Shockwave(Float2 at, Color4 color, float reach = 0.0f);
    void Glitch(float seconds);
    void SlowMotion(float seconds, Color4 color);
    // Each frame: cinematic camera shot; weight 0 gives the normal camera.
    void SetShot(const CameraShot& shot) { _shot = shot; }
    // Field camera: shot + shake / punch / kick / motion; draw with paint.Transform(camera.Transform()).
    [[nodiscard]] CameraView WorldCamera(float timeSeconds) const;
    // Inside the world camera, over the field's objects.
    void DrawWorldLayer(Painter& paint) const;
    // Without the camera, over the viewport.
    void DrawOverlay(Painter& paint) const;

private:
    FloatRect _area;
    FloatRect _viewArea;
    FxStyle _style;
    ParticleSystem2D _particles;
    FloatingText2D _texts;
    RingEffects2D _rings;
    CameraEffects2D _camera;
    OverlayEffects2D _overlays;
    PulseOverlay2D _pulse;
    ShockwaveEffects2D _shockwaves;
    GlitchEffects2D _glitch;
    Color4 _motionColor{};
    float _slowMotionAge = 0.0f;
    float _slowMotionDuration = 0.0f;
    Color4 _slowMotionColor{};
    CameraShot _shot;
};
