#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Fx/Camera/CameraShot.h"
#include "Fx/FxCue.h"
#include "Fx/FxCueSet.h"
#include "Fx/FxQuality.h"
#include "Fx/FxStyle.h"
#include "Camera/CameraView.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;
class TimeScale;

// What a cue plays on: where, in which colour, which way and with what text.
struct FxPlay {
    Float2 at{};
    // How fast the thing it happens to moves, canvas units/second: particles take ParticleEmission::inherit of it.
    Float2 velocity{};
    Color4 color{255, 255, 255, 255};
    Float2 facing{1.0f, 0.0f};
    std::string_view text{};
    // How big this one is: scales particle counts, shake, punch, kick, ring and reach sizes, a flash's opacity and a
    // light flash's strength.
    // Times, texts and colours stay. 0 plays nothing.
    float intensity = 1.0f;
};

// Ready-made effects stage for a game field: particles, flying text, rings, shockwaves, camera (shake, punch, kick,
// motion, cinematic shot), flash / banner / toast, pulse, glitch and the look of slow motion. Positions use canvas
// coordinates; styles are supplied at setup (usually CurrentTheme().fx), and an invalid one throws there. 'area' is
// where effects play; 'viewArea' is the viewport covered by edge effects (vignette, flash).
//
// Two ways to ask: Play a named cue for what an event looks like, or call one effect by its method.
//
// Lifetimes and cue delays run on game time, so a hitstop holds them. Banners, toasts and how long slow motion's look
// lasts run on real time. Shake, speed lines and glitch patterns derive from the drawing clock (Painter::Time).
class FxStage {
public:
    FxStage(const FloatRect& area, const FloatRect& viewArea, const FxStyle& style);
    // The player's effect choice (Game::Effects()), for every effect here.
    void SetQuality(const FxQuality& quality);
    // The choice last set: a Flicker beside the stage takes its `flashes`.
    [[nodiscard]] const FxQuality& Quality() const { return _quality; }
    // The cues Play finds by name; DefaultFxCues() until set.
    void SetCues(const FxCueSet& cues);
    [[nodiscard]] const FxCueSet& Cues() const { return _cues; }
    // What cues with FxHitstop or FxSlowMotion change, usually Game::Time(). It must outlive the stage; nullptr unsets
    // it.
    void SetTime(TimeScale* time) { _time = time; }
    // Each frame from the scene's UpdateReal, which runs in a hitstop and a pause too: banners, toasts and how long
    // slow motion looks slow, which reading and TimeScale need on real time.
    void UpdateReal(float realSeconds);
    // Each frame from the scene's Update: lifetimes, cue delays and cooldowns on game time, so a hitstop holds them.
    // Each clock moves only its own parts, so a scene whose hooks both get real seconds moves nothing twice.
    void Update(float gameSeconds);
    // Also forgets cue actions still waiting and cooldowns.
    void Clear();

    // Plays a cue from Cues(); false if its cooldown has not passed. An unknown name throws. A cue that changes time
    // throws without SetTime, also later if its time comes after SetTime(nullptr).
    bool Play(std::string_view name, const FxPlay& play = {});
    void Play(const FxCue& cue, const FxPlay& play = {});

    // A source is a position, or a position and a velocity for particles that inherit it.
    void Burst(const ParticleSource& from, Color4 color, int count, ParticleRecipe recipe = ParticleRecipe::Soft);
    // A burst of the game's own; 'draw' replaces the layers' shapes, for sprites.
    void Burst(const ParticleSource& from, Color4 color, int count, const ParticleBurst& burst,
               const ParticleCustomDraw& draw = {});
    // Each frame, for a stream the caller owns (a trail, smoke): runs it on the stage's particles. See
    // ParticleEmitter; the theme's streams are in style.particles (trail, smoke, flame, embers).
    void Emit(ParticleEmitter& emitter, Float2 at, Color4 color, float seconds);
    // 'facing': movement direction; sparks scatter behind it.
    void Dash(const ParticleSource& from, Float2 facing, Color4 color);
    void Steam(const ParticleSource& from, Color4 color);
    void FloatText(Float2 at, const std::string& text, Color4 color);
    // Flying text of 'size' with a popping entrance.
    void PopText(Float2 at, const std::string& text, Color4 color, float size);
    void Ring(Float2 at, Color4 color);
    // A mark left on the ground (a scorch, a splash, a falling thing's shadow); see DrawGroundLayer.
    void Mark(Float2 at, Color4 color, const MarkStyle& style = {});
    void Toast(const std::string& text, Color4 color);
    void Flash(Color4 color);
    // A short light; see LightFlashes. It shares the flash limit with Flash: starting together, they count as one.
    void LightFlash(Float2 at, Color4 color, const LightFlashStyle& style);
    // A bolt of lightning from above the area down to 'at'. It shares the flash limit too.
    void Bolt(Float2 at, Color4 color, const BoltStyle& style = {});
    void Banner(const std::string& text);
    // Adds to the shake, up to 1; see CameraEffects::Shake.
    void Shake(float amount);
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
    // Field camera: shot + shake / punch / kick / motion; draw with paint.Camera(view).
    [[nodiscard]] CameraView WorldCamera(const Painter& paint) const;
    // The same over a game camera (Camera2D::View), one that follows or scrolls: effect positions are then world
    // positions. A shot blends from it by its weight, in world positions too, and stays at the view's place on the
    // canvas, so each view of a split screen keeps its own. With keepInArena the shot stays inside 'limits', usually
    // the camera's (Camera2D::Settings().limits).
    [[nodiscard]] CameraView WorldCamera(const Painter& paint, const CameraView& base,
                                         const std::optional<FloatRect>& limits = std::nullopt) const;
    // The short lights playing now, for LightFrame::flashes. Valid until the next call that changes the stage.
    [[nodiscard]] LightFlashFrame LightFlashes() const { return _flashLights.Frame(); }
    // Inside the world camera, over the floor and under the field's objects: marks.
    void DrawGroundLayer(Painter& paint) const;
    // Inside the world camera, over the field's objects.
    void DrawWorldLayer(Painter& paint) const;
    // Without the camera, over the viewport.
    void DrawOverlay(Painter& paint) const;

private:
    struct Target {
        Color4 color;
        Float2 at;
        Float2 velocity;
        Float2 facing;
        std::string_view text;
        float intensity;
    };
    struct Waiting {
        float seconds;
        FxAction action;
        Color4 color;
        Float2 at;
        Float2 velocity;
        Float2 facing;
        std::string text;
        float intensity;
    };
    struct Cooling {
        std::string name;
        float seconds;
    };
    void Run(const FxAction& action, const Target& target);

    FloatRect _area;
    FloatRect _viewArea;
    FxStyle _style;
    FxCueSet _cues;
    FxQuality _quality;
    TimeScale* _time = nullptr;
    std::vector<Waiting> _waiting;
    std::vector<Cooling> _cooling;
    ParticleSystem _particles;
    FloatingTexts _texts;
    Rings _rings;
    Marks _marks;
    CameraEffects _camera;
    ScreenFlash _flash;
    FlashLights _flashLights;
    Bolts _bolts;
    Announcements _announcements;
    PulseOverlay _pulse;
    Shockwaves _shockwaves;
    ScreenGlitch _glitch;
    SlowMotionOverlay _slowMotion;
    Color4 _motionColor{};
    CameraShot _shot;
};
