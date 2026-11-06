#include "Fx/FxStage.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>
#include <stdexcept>
#include <variant>
#include "Animation/Easing.h"
#include "Fx/Internals/FxChecks.h"
#include "Graphics/Draw/BackgroundDraw.h"
#include "Graphics/Draw/SpeedLines.h"
#include "Graphics/Paint/Painter.h"
#include "Runtime/TimeScale.h"
#include "Utilities/Math/Scalar.h"

namespace {
// A visit with these does not compile when an FxAction has no case.
template <class... Cases> struct Overloaded : Cases... {
    using Cases::operator()...;
};
template <class... Cases> Overloaded(Cases...) -> Overloaded<Cases...>;

void Require(bool valid, const char* part) {
    if (!valid) throw std::invalid_argument(std::format("Invalid effect style: {}", part));
}
// Here rather than at the first use: a mistake in a theme should not wait for a boss's death.
const FxStyle& Checked(const FxStyle& style) {
    Require(IsValid(style.particles), "particles");
    Require(IsValid(style.camera), "camera");
    Require(IsValid(style.text), "text");
    Require(FxNotNegative(style.textPopSeconds), "textPopSeconds");
    Require(IsValid(style.ring), "ring");
    Require(IsValid(style.flash), "flash");
    Require(IsValid(style.banner), "banner");
    Require(IsValid(style.toast), "toast");
    Require(IsValid(style.speedLines), "speedLines");
    Require(IsValid(style.pulse), "pulse");
    Require(IsValid(style.shockwave), "shockwave");
    Require(IsValid(style.glitch), "glitch");
    Require(IsValid(style.slowMotion), "slowMotion");
    Require(FxNotNegative(style.vignetteSize), "vignetteSize");
    return style;
}
// At least one of a request: 'intensity' is above 0.
int Scaled(int count, float intensity) {
    if (count <= 0) return count;
    return std::max(1, static_cast<int>(std::lround(static_cast<float>(count) * intensity)));
}
} // namespace

FxStage::FxStage(const FloatRect& area, const FloatRect& viewArea, const FxStyle& style)
    : _area(area)
    , _viewArea(viewArea)
    , _style(Checked(style))
    , _cues(DefaultFxCues())
    , _particles({.capacity = style.particleCapacity})
    , _camera(style.camera)
    , _announcements(style.toastCapacity)
    , _pulse(style.pulse) {
    _style.text.text.maxWidth = area.width;
}
void FxStage::SetQuality(const FxQuality& quality) {
    _quality = quality;
    _particles.SetQuality(quality);
    _camera.SetQuality(quality);
    _flash.SetQuality(quality);
    _flashLights.SetQuality(quality);
    _bolts.SetQuality(quality);
    _announcements.SetQuality(quality);
    _pulse.SetQuality(quality);
    _shockwaves.SetQuality(quality);
    _glitch.SetQuality(quality);
    _slowMotion.SetQuality(quality);
}
void FxStage::SetCues(const FxCueSet& cues) {
    _cues = cues;
    _cooling.clear();
}
void FxStage::UpdateReal(float realSeconds) {
    _announcements.Update(realSeconds);
    _slowMotion.UpdateReal(realSeconds);
}
void FxStage::Update(float gameSeconds) {
    _slowMotion.Update(gameSeconds);
    if (!(gameSeconds > 0.0f) || !std::isfinite(gameSeconds)) return;
    for (Cooling& cooling : _cooling)
        cooling.seconds -= gameSeconds;
    std::erase_if(_cooling, [](const Cooling& cooling) { return cooling.seconds <= 0.0f; });
    for (Waiting& waiting : _waiting)
        waiting.seconds -= gameSeconds;
    // Out of the list first: one that throws must not throw again every frame.
    const auto due = std::ranges::stable_partition(_waiting, [](const Waiting& w) { return w.seconds > 0.0f; });
    const std::vector<Waiting> running(std::make_move_iterator(due.begin()), std::make_move_iterator(due.end()));
    _waiting.erase(due.begin(), due.end());
    for (const Waiting& waiting : running)
        Run(waiting.action,
            {waiting.color, waiting.at, waiting.velocity, waiting.facing, waiting.text, waiting.intensity});
    _particles.Update(gameSeconds);
    _texts.Update(gameSeconds);
    _rings.Update(gameSeconds);
    _marks.Update(gameSeconds);
    _camera.Update(gameSeconds);
    _flash.Update(gameSeconds);
    _flashLights.Update(gameSeconds);
    _bolts.Update(gameSeconds);
    _pulse.Update(gameSeconds);
    _shockwaves.Update(gameSeconds);
    _glitch.Update(gameSeconds);
}
void FxStage::Clear() {
    _waiting.clear();
    _cooling.clear();
    _particles.Clear();
    _texts.Clear();
    _rings.Clear();
    _marks.Clear();
    _camera.Clear();
    _flash.Clear();
    _flashLights.Clear();
    _bolts.Clear();
    _announcements.Clear();
    _pulse.Clear();
    _shockwaves.Clear();
    _glitch.Clear();
    _slowMotion.Clear();
    _motionColor = {};
    _shot = {};
}

bool FxStage::Play(std::string_view name, const FxPlay& play) {
    const FxCue& cue = _cues.Get(name);
    if (std::ranges::any_of(_cooling, [&](const Cooling& cooling) { return cooling.name == name; })) return false;
    Play(cue, play);
    const float cooldown = _cues.Cooldown(name);
    if (cooldown > 0.0f && play.intensity > 0.0f) _cooling.push_back({std::string(name), cooldown});
    return true;
}
void FxStage::Play(const FxCue& cue, const FxPlay& play) {
    if (!FxNotNegative(play.intensity)) throw std::invalid_argument("A cue's intensity must be nonnegative");
    if (cue.UsesTime() && _time == nullptr) throw std::invalid_argument("This cue changes time: call FxStage::SetTime");
    if (play.intensity == 0.0f) return;
    for (const FxCueEntry& entry : cue.Entries()) {
        const Color4 color = entry.color.value_or(play.color);
        if (entry.at > 0.0f)
            _waiting.push_back({entry.at, entry.action, color, play.at, play.velocity, play.facing,
                                std::string(play.text), play.intensity});
        else
            Run(entry.action, {color, play.at, play.velocity, play.facing, play.text, play.intensity});
    }
}
void FxStage::Run(const FxAction& action, const Target& target) {
    const auto textOf = [&](const std::string& own) { return own.empty() ? std::string(target.text) : own; };
    const auto time = [this]() -> TimeScale& {
        if (_time == nullptr) throw std::invalid_argument("This cue changes time: call FxStage::SetTime");
        return *_time;
    };
    const float scale = target.intensity;
    const ParticleSource source{target.at, target.velocity};
    std::visit(
        Overloaded{
            [&](const FxBurst& a) { Burst(source, target.color, Scaled(a.count, scale), a.recipe); },
            [&](const FxParticles& a) { Burst(source, target.color, Scaled(a.count, scale), a.burst); },
            [&](const FxDash&) { Dash(source, target.facing, target.color); },
            [&](const FxSteam&) { Steam(source, target.color); },
            [&](const FxFloatText& a) { FloatText(target.at, textOf(a.text), target.color); },
            [&](const FxPopText& a) { PopText(target.at, textOf(a.text), target.color, a.size); },
            [&](const FxRing&) {
                RingStyle style = _style.ring;
                style.startRadius *= scale;
                style.endRadius *= scale;
                _rings.Emit(target.at, target.color, style);
            },
            [&](const FxShockwave& a) { Shockwave(target.at, target.color, a.reach * scale); },
            [&](const FxFlash&) {
                FlashStyle style = _style.flash;
                style.opacity *= std::min(scale, 1.0f);
                _flash.Flash(target.color, style);
            },
            [&](const FxLightFlash& a) {
                LightFlashStyle style = a;
                style.strength *= scale;
                LightFlash(target.at, target.color, style);
            },
            [&](const FxBolt& a) { Bolt(target.at, target.color, a); },
            [&](const FxMark& a) {
                MarkStyle style = a;
                style.radius *= scale;
                Mark(target.at, target.color, style);
            },
            [&](const FxBanner& a) { Banner(textOf(a.text)); },
            [&](const FxToast& a) { Toast(textOf(a.text), target.color); },
            [&](const FxShake& a) { Shake(a.amount * scale); },
            [&](const FxPunch& a) { Punch(a.amount * scale, target.at); },
            [&](const FxKick& a) { Kick(target.facing * (a.strength * scale)); },
            [&](const FxGlitch& a) { Glitch(a.seconds); },
            [&](const FxHitstop& a) { time().Hitstop(a.seconds); },
            [&](const FxSlowMotion& a) {
                time().SlowMo(a.seconds, a.startScale, a.endScale);
                SlowMotionLook(a.seconds, target.color);
            },
        },
        action);
}

void FxStage::Burst(const ParticleSource& from, Color4 color, int count, ParticleRecipe recipe) {
    EmitBurst(_particles, from, color, count, _style.particles.Burst(recipe));
}
void FxStage::Burst(const ParticleSource& from, Color4 color, int count, const ParticleBurst& burst,
                    const ParticleCustomDraw& draw) {
    EmitBurst(_particles, from, color, count, burst, draw);
}
void FxStage::Emit(ParticleEmitter& emitter, Float2 at, Color4 color, float seconds) {
    emitter.Emit(_particles, at, color, seconds);
}
void FxStage::Dash(const ParticleSource& from, Float2 facing, Color4 color) {
    EmitLayer(_particles, from, color, _style.particles.dashCount, AimedBehind(_style.particles.dash, facing));
}
void FxStage::Steam(const ParticleSource& from, Color4 color) {
    const auto& recipes = _style.particles;
    const Color4 steam = Mix(color, Color4{255, 255, 255, 255}, recipes.steamWhiten);
    EmitLayer(_particles, from, WithAlpha(steam, recipes.steamOpacity), recipes.steamCount, recipes.steam);
}
void FxStage::FloatText(Float2 at, const std::string& text, Color4 color) { _texts.Emit(at, text, color, _style.text); }
void FxStage::PopText(Float2 at, const std::string& text, Color4 color, float size) {
    auto style = _style.text;
    style.text.size = size;
    style.popSeconds = _style.textPopSeconds;
    _texts.Emit(at, text, color, style);
}
void FxStage::Bolt(Float2 at, Color4 color, const BoltStyle& style) {
    _bolts.Strike(_flash.Limit(), at, _area, color, style);
}
void FxStage::Ring(Float2 at, Color4 color) { _rings.Emit(at, color, _style.ring); }
void FxStage::Mark(Float2 at, Color4 color, const MarkStyle& style) { _marks.Leave(at, color, style); }
void FxStage::Toast(const std::string& text, Color4 color) { _announcements.Toast(text, color, _style.toast); }
void FxStage::Flash(Color4 color) { _flash.Flash(color, _style.flash); }
void FxStage::LightFlash(Float2 at, Color4 color, const LightFlashStyle& style) {
    _flashLights.Start(_flash.Limit(), at, color, style);
}
void FxStage::Banner(const std::string& text) { _announcements.Banner(text, _style.banner); }
void FxStage::Shake(float seconds) { _camera.Shake(seconds); }
void FxStage::Punch(float amount, std::optional<Float2> focus) { _camera.Punch(amount, focus); }
void FxStage::Kick(Float2 direction) { _camera.Kick(direction); }
void FxStage::SetMotion(float intensity, Color4 color, Float2 facing) {
    _motionColor = color;
    _camera.SetMotion(intensity, facing);
}
void FxStage::SetPulse(bool active, Color4 color) { _pulse.SetActive(active, color); }
void FxStage::Shockwave(Float2 at, Color4 color, float reach) { _shockwaves.Emit(at, color, _style.shockwave, reach); }
void FxStage::Glitch(float seconds) { _glitch.Trigger(seconds, _style.glitch); }
void FxStage::SlowMotionLook(float seconds, Color4 color) { _slowMotion.Trigger(seconds, color, _style.slowMotion); }
CameraView FxStage::WorldCamera(const Painter& paint) const {
    return _camera.Apply(ShotCamera(_shot, _area, _area), static_cast<float>(paint.Time()));
}
CameraView FxStage::WorldCamera(const Painter& paint, const CameraView& base,
                                const std::optional<FloatRect>& limits) const {
    CameraView camera = base;
    const float weight = Clamp01(_shot.weight);
    if (weight > 0.0f) {
        camera.target = base.target + (_shot.focus - base.target) * weight;
        camera.rotation = Mix(base.rotation, _shot.rotation, weight);
        camera.zoom = Mix(base.zoom, _shot.zoom, weight);
        if (_shot.keepInArena && limits) camera = camera.ConfinedTo(*limits);
    }
    return _camera.Apply(camera, static_cast<float>(paint.Time()));
}
void FxStage::DrawGroundLayer(Painter& paint) const { _marks.Draw(paint); }
void FxStage::DrawWorldLayer(Painter& paint) const {
    _bolts.Draw(paint);
    _rings.Draw(paint);
    _particles.Draw(paint);
    _texts.Draw(paint);
    _shockwaves.Draw(paint, _area);
}
void FxStage::DrawOverlay(Painter& paint) const {
    const auto time = static_cast<float>(paint.Time());
    _pulse.DrawTint(paint, _area);
    _pulse.DrawVignette(paint, _viewArea);
    DrawVignette(paint, _viewArea, _style.vignetteSize, _style.vignetteColor);
    DrawSpeedLines(paint, _area, _camera.MotionIntensity(), _motionColor, time, _style.speedLines);
    _slowMotion.Draw(paint, _area, _viewArea);
    _flash.Draw(paint, _viewArea);
    _announcements.Draw(paint, _area, _viewArea);
    _glitch.Draw(paint, _area, paint.Time());
}
