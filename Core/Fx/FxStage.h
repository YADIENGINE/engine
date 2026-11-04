#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Fx/CameraShot.h"
#include "Fx/FxCue.h"
#include "Fx/FxCueSet.h"
#include "Fx/FxQuality.h"
#include "Fx/FxStyle.h"
#include "Utilities/CameraView.h"
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRect.h"

class Painter;
class TimeScale;

// What a cue plays on: where, in which colour, which way and with what text.
struct FxPlay {
    Float2 at{};
    Color4 color{255, 255, 255, 255};
    Float2 facing{1.0f, 0.0f};
    std::string_view text{};
};

// Ready-made effects stage for a game field: particles, flying text, rings, shockwaves, camera (shake, punch, kick,
// motion, cinematic shot), flash / banner / toast, pulse, glitch and the look of slow motion. Positions use canvas
// coordinates; styles are supplied at setup (usually CurrentTheme().fx). 'area' is where effects play; 'viewArea' is
// the viewport covered by edge effects (vignette, flash).
//
// Two ways to ask: Play a named cue for what an event looks like, or call one effect by its method.
//
// Lifetimes, and cue delays, advance with Update time, so a hitstop holds them. Shake, speed lines and glitch patterns
// derive from the drawing clock (Painter::Time).
class FxStage {
public:
    FxStage(const FloatRect& area, const FloatRect& viewArea, const FxStyle& style);
    // The player's effect choice (Game::Effects()), for every effect here.
    void SetQuality(const FxQuality& quality);
    // The cues Play finds by name; DefaultFxCues() until set.
    void SetCues(const FxCueSet& cues) { _cues = cues; }
    [[nodiscard]] const FxCueSet& Cues() const { return _cues; }
    // What cues with FxHitstop or FxSlowMotion change, usually Game::Time(). It must outlive the stage; nullptr unsets
    // it.
    void SetTime(TimeScale* time) { _time = time; }
    void Update(float deltaSeconds);
    // Also forgets cue actions still waiting.
    void Clear();

    // Plays a cue from Cues(); an unknown name throws. A cue that changes time throws without SetTime.
    void Play(std::string_view name, const FxPlay& play = {});
    void Play(const FxCue& cue, const FxPlay& play = {});

    void Burst(Float2 at, Color4 color, int count, ParticleRecipe recipe = ParticleRecipe::Soft);
    // A burst of the game's own.
    void Burst(Float2 at, Color4 color, int count, const ParticleBurst& burst);
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
    // Only how slow motion looks; TimeScale::SlowMo slows time, and FxSlowMotion in a cue does both.
    void SlowMotionLook(float seconds, Color4 color);
    // Each frame: cinematic camera shot; weight 0 gives the normal camera. CameraShot can be tweened or put in a clip.
    void SetShot(const CameraShot& shot) { _shot = shot; }
    // Field camera: shot + shake / punch / kick / motion; draw with paint.Transform(camera.Transform()).
    [[nodiscard]] CameraView WorldCamera(const Painter& paint) const;
    // Inside the world camera, over the field's objects.
    void DrawWorldLayer(Painter& paint) const;
    // Without the camera, over the viewport.
    void DrawOverlay(Painter& paint) const;

private:
    struct Target {
        Color4 color;
        Float2 at;
        Float2 facing;
        std::string_view text;
    };
    struct Waiting {
        float seconds;
        FxAction action;
        Color4 color;
        Float2 at;
        Float2 facing;
        std::string text;
    };
    void Run(const FxAction& action, const Target& target);

    FloatRect _area;
    FloatRect _viewArea;
    FxStyle _style;
    FxCueSet _cues;
    TimeScale* _time = nullptr;
    std::vector<Waiting> _waiting;
    ParticleSystem _particles;
    FloatingTexts _texts;
    Rings _rings;
    CameraEffects _camera;
    ScreenFlash _flash;
    Announcements _announcements;
    PulseOverlay _pulse;
    Shockwaves _shockwaves;
    ScreenGlitch _glitch;
    SlowMotionOverlay _slowMotion;
    Color4 _motionColor{};
    CameraShot _shot;
};
