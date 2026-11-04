#include "Fx/FxStage.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include "Graphics/Draw/BackgroundDraw.h"
#include "Graphics/Draw/SpeedLines.h"
#include "Graphics/Paint/Painter.h"
#include "Runtime/TimeScale.h"

FxStage::FxStage(const FloatRect& area, const FloatRect& viewArea, const FxStyle& style)
    : _area(area)
    , _viewArea(viewArea)
    , _style(style)
    , _cues(DefaultFxCues())
    , _particles({.capacity = style.particleCapacity})
    , _camera(style.camera)
    , _announcements(style.toastCapacity)
    , _pulse(style.pulse) {
    _style.text.text.maxWidth = area.width;
}
void FxStage::SetQuality(const FxQuality& quality) {
    _particles.SetQuality(quality);
    _camera.SetQuality(quality);
    _flash.SetQuality(quality);
    _pulse.SetQuality(quality);
    _glitch.SetQuality(quality);
    _slowMotion.SetQuality(quality);
}
void FxStage::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    for (Waiting& waiting : _waiting) {
        waiting.seconds -= deltaSeconds;
        if (waiting.seconds <= 0.0f) Run(waiting.action, {waiting.color, waiting.at, waiting.facing, waiting.text});
    }
    std::erase_if(_waiting, [](const Waiting& waiting) { return waiting.seconds <= 0.0f; });
    _particles.Update(deltaSeconds);
    _texts.Update(deltaSeconds);
    _rings.Update(deltaSeconds);
    _camera.Update(deltaSeconds);
    _flash.Update(deltaSeconds);
    _announcements.Update(deltaSeconds);
    _pulse.Update(deltaSeconds);
    _shockwaves.Update(deltaSeconds);
    _glitch.Update(deltaSeconds);
    _slowMotion.Update(deltaSeconds);
}
void FxStage::Clear() {
    _waiting.clear();
    _particles.Clear();
    _texts.Clear();
    _rings.Clear();
    _camera.Clear();
    _flash.Clear();
    _announcements.Clear();
    _pulse.Clear();
    _shockwaves.Clear();
    _glitch.Clear();
    _slowMotion.Clear();
    _motionColor = {};
    _shot = {};
}

void FxStage::Play(std::string_view name, const FxPlay& play) { Play(_cues.Get(name), play); }
void FxStage::Play(const FxCue& cue, const FxPlay& play) {
    if (cue.UsesTime() && _time == nullptr) throw std::invalid_argument("This cue changes time: call FxStage::SetTime");
    for (const FxCueEntry& entry : cue.Entries()) {
        const Color4 color = entry.color.value_or(play.color);
        if (entry.at > 0.0f)
            _waiting.push_back({entry.at, entry.action, color, play.at, play.facing, std::string(play.text)});
        else
            Run(entry.action, {color, play.at, play.facing, play.text});
    }
}
void FxStage::Run(const FxAction& action, const Target& target) {
    const auto textOf = [&](const std::string& own) { return own.empty() ? std::string(target.text) : own; };
    std::visit(
        [&](const auto& a) {
            using A = std::decay_t<decltype(a)>;
            if constexpr (std::same_as<A, FxBurst>)
                Burst(target.at, target.color, a.count, a.recipe);
            else if constexpr (std::same_as<A, FxParticles>)
                Burst(target.at, target.color, a.count, a.burst);
            else if constexpr (std::same_as<A, FxTrail>)
                Trail(target.at, target.color);
            else if constexpr (std::same_as<A, FxDash>)
                Dash(target.at, target.facing, target.color);
            else if constexpr (std::same_as<A, FxSteam>)
                Steam(target.at, target.color);
            else if constexpr (std::same_as<A, FxFloatText>)
                FloatText(target.at, textOf(a.text), target.color);
            else if constexpr (std::same_as<A, FxPopText>)
                PopText(target.at, textOf(a.text), target.color, a.size);
            else if constexpr (std::same_as<A, FxRing>)
                Ring(target.at, target.color);
            else if constexpr (std::same_as<A, FxShockwave>)
                Shockwave(target.at, target.color, a.reach);
            else if constexpr (std::same_as<A, FxFlash>)
                Flash(target.color);
            else if constexpr (std::same_as<A, FxBanner>)
                Banner(textOf(a.text));
            else if constexpr (std::same_as<A, FxToast>)
                Toast(textOf(a.text), target.color);
            else if constexpr (std::same_as<A, FxShake>)
                Shake(a.seconds);
            else if constexpr (std::same_as<A, FxPunch>)
                Punch(a.amount, target.at);
            else if constexpr (std::same_as<A, FxKick>)
                Kick(target.facing * a.strength);
            else if constexpr (std::same_as<A, FxGlitch>)
                Glitch(a.seconds);
            else if constexpr (std::same_as<A, FxHitstop>)
                _time->Hitstop(a.seconds);
            else if constexpr (std::same_as<A, FxSlowMotion>) {
                _time->SlowMo(a.seconds, a.startScale, a.endScale);
                SlowMotionLook(a.seconds, target.color);
            }
        },
        action);
}

void FxStage::Burst(Float2 at, Color4 color, int count, ParticleRecipe recipe) {
    EmitBurst(_particles, at, color, count, _style.particles.Burst(recipe));
}
void FxStage::Burst(Float2 at, Color4 color, int count, const ParticleBurst& burst) {
    EmitBurst(_particles, at, color, count, burst);
}
void FxStage::Trail(Float2 at, Color4 color) {
    EmitLayer(_particles, at, color, _style.particles.trailCount, _style.particles.trail);
}
void FxStage::Dash(Float2 at, Float2 facing, Color4 color) {
    EmitLayer(_particles, at, color, _style.particles.dashCount, AimedBehind(_style.particles.dash, facing));
}
void FxStage::Steam(Float2 at, Color4 color) {
    const auto& recipes = _style.particles;
    const Color4 steam = Mix(color, Color4{255, 255, 255, 255}, recipes.steamWhiten);
    EmitLayer(_particles, at, WithAlpha(steam, recipes.steamOpacity), recipes.steamCount, recipes.steam);
}
void FxStage::FloatText(Float2 at, const std::string& text, Color4 color) { _texts.Emit(at, text, color, _style.text); }
void FxStage::PopText(Float2 at, const std::string& text, Color4 color, float size) {
    auto style = _style.text;
    style.text.size = size;
    style.popSeconds = _style.textPopSeconds;
    _texts.Emit(at, text, color, style);
}
void FxStage::Ring(Float2 at, Color4 color) { _rings.Emit(at, color, _style.ring); }
void FxStage::Toast(const std::string& text, Color4 color) { _announcements.Toast(text, color, _style.toast); }
void FxStage::Flash(Color4 color) { _flash.Flash(color, _style.flash); }
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
void FxStage::DrawWorldLayer(Painter& paint) const {
    _pulse.DrawTint(paint, _area);
    _rings.Draw(paint);
    _particles.Draw(paint);
    _texts.Draw(paint);
    _shockwaves.Draw(paint, _area);
}
void FxStage::DrawOverlay(Painter& paint) const {
    const auto time = static_cast<float>(paint.Time());
    _pulse.DrawVignette(paint, _viewArea);
    DrawVignette(paint, _viewArea, _style.vignetteSize, _style.vignetteColor);
    DrawSpeedLines(paint, _area, _camera.MotionIntensity(), _motionColor, time, _style.speedLines);
    _slowMotion.Draw(paint, _area, _viewArea);
    _flash.Draw(paint, _viewArea);
    _announcements.Draw(paint, _area, _viewArea);
    _glitch.Draw(paint, _area, paint.Time());
}
