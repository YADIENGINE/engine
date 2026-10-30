#include "Fx/FxStage.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Draw/BackgroundDraw.h"
#include "Graphics/Draw/DrawUtils.h"
#include "Graphics/Anim/Easing.h"

FxStage::FxStage(const Rect& area, const Rect& viewArea, const FxStyle& style)
    : _area(area)
    , _viewArea(viewArea)
    , _style(style)
    , _particles({.capacity = style.particleCapacity})
    , _camera(style.camera)
    , _overlays(style.toastCapacity)
    , _pulse(style.pulse)
    , _shockwaves(style.shockwave)
    , _glitch(style.glitch) {
    _style.text.text.maxWidth = area.width;
}
void FxStage::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    _particles.Update(deltaSeconds);
    _texts.Update(deltaSeconds);
    _rings.Update(deltaSeconds);
    _camera.Update(deltaSeconds);
    _overlays.Update(deltaSeconds);
    _pulse.Update(deltaSeconds);
    _shockwaves.Update(deltaSeconds);
    _glitch.Update(deltaSeconds);
    _slowMotionAge = std::min(_slowMotionAge + deltaSeconds, _slowMotionDuration);
}
void FxStage::Clear() {
    _particles.Clear();
    _texts.Clear();
    _rings.Clear();
    _camera.Clear();
    _overlays.Clear();
    _pulse.Clear();
    _shockwaves.Clear();
    _glitch.Clear();
    _motionColor = {};
    _slowMotionAge = 0.0f;
    _slowMotionDuration = 0.0f;
    _slowMotionColor = {};
    _shot = {};
}
void FxStage::Burst(Vector2 at, Color color, int count, ParticleRecipe recipe) {
    EmitBurst(_particles, at, color, count, recipe, _style.particles);
}
void FxStage::Trail(Vector2 at, Color color) {
    EmitLayer(_particles, at, color, _style.particles.trailCount, TrailRecipe(_style.particles));
}
void FxStage::Dash(Vector2 at, Vector2 facing, Color color) {
    EmitLayer(_particles, at, color, _style.particles.dashCount, DashRecipe(facing, _style.particles));
}
void FxStage::Steam(Vector2 at, Color color) {
    const auto& tuning = _style.particles;
    EmitLayer(_particles, at, Fade(ColorLerp(color, WHITE, tuning.steamWhiten), tuning.steamOpacity), tuning.steamCount,
              SteamRecipe(tuning));
}
void FxStage::FloatText(Vector2 at, const std::string& text, Color color) { _texts.Emit(at, text, color, _style.text); }
void FxStage::PopText(Vector2 at, const std::string& text, Color color, int fontSize) {
    auto style = _style.text;
    style.text.fontSize = fontSize;
    style.popSeconds = _style.textPopSeconds;
    _texts.Emit(at, text, color, style);
}
void FxStage::Ring(Vector2 at, Color color) { _rings.Emit(at, color, _style.ring); }
void FxStage::Toast(const std::string& text, Color color) { _overlays.Toast(text, color, _style.toast); }
void FxStage::Flash(Color color) { _overlays.Flash(color, _style.flash); }
void FxStage::Banner(const std::string& text) { _overlays.Banner(text, _style.banner); }
void FxStage::Shake(float seconds) { _camera.Shake(seconds); }
void FxStage::Punch(float amount, std::optional<Vector2> focus) { _camera.Punch(amount, focus); }
void FxStage::Kick(Vector2 direction) { _camera.Kick(direction); }
void FxStage::SetMotion(float intensity, Color color, Vector2 facing) {
    _motionColor = color;
    _camera.SetMotion(intensity, facing);
}
void FxStage::SetPulse(bool active, Color color) { _pulse.SetActive(active, color); }
void FxStage::Shockwave(Vector2 at, Color color, float reach) { _shockwaves.Emit(at, color, reach); }
void FxStage::Glitch(float seconds) { _glitch.Trigger(seconds); }
void FxStage::SlowMotion(float seconds, Color color) {
    _slowMotionAge = 0.0f;
    _slowMotionDuration = std::max(0.0f, seconds);
    _slowMotionColor = color;
}
Camera2D FxStage::WorldCamera(float timeSeconds) const {
    const Camera2D base = ShotCamera(_shot, ToRectangle(_area), ToRectangle(_area));
    return _camera.Apply(base, timeSeconds);
}
void FxStage::DrawWorldLayer(const TextRenderer& text) const {
    _pulse.DrawTint(_area);
    _rings.Draw();
    _particles.Draw();
    _texts.Draw(text);
    _shockwaves.Draw(_area);
}
void FxStage::DrawOverlay(const TextRenderer& text, float timeSeconds) const {
    _pulse.DrawVignette(_viewArea);
    DrawVignette(_viewArea, _style.vignetteSize, _style.vignetteColor);
    DrawSpeedLines(_area, _camera.MotionIntensity(), _motionColor, timeSeconds, _style.speedLines);
    if (_slowMotionAge < _slowMotionDuration) {
        const auto& slow = _style.slowMotion;
        const float fadeStart = _slowMotionDuration * slow.fadeStart;
        const float strength = 1.0f - EaseOutCubic(Progress01(_slowMotionAge, fadeStart, _slowMotionDuration));
        DrawRectangleRec(ToRectangle(_area), Fade(_slowMotionColor, slow.tintAlpha * strength));
        DrawVignette(_viewArea, slow.vignetteSize, Fade(_slowMotionColor, slow.vignetteAlpha * strength));
        DrawSpeedLines(_area, strength, _slowMotionColor, _slowMotionAge, slow.lines);
    }
    _overlays.Draw(text, _area, _viewArea);
    _glitch.Draw(_area, timeSeconds);
}
