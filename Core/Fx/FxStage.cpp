#include "Fx/FxStage.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Anim/Easing.h"
#include "Graphics/Draw/BackgroundDraw.h"
#include "Graphics/Paint/Painter.h"

FxStage::FxStage(const FloatRect& area, const FloatRect& viewArea, const FxStyle& style)
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
void FxStage::SetQuality(const FxQuality& quality) {
    _particles.SetQuality(quality);
    _camera.SetQuality(quality);
    _overlays.SetQuality(quality);
    _glitch.SetQuality(quality);
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
void FxStage::Burst(Float2 at, Color4 color, int count, ParticleRecipe recipe) {
    EmitBurst(_particles, at, color, count, recipe, _style.particles);
}
void FxStage::Trail(Float2 at, Color4 color) {
    EmitLayer(_particles, at, color, _style.particles.trailCount, TrailRecipe(_style.particles));
}
void FxStage::Dash(Float2 at, Float2 facing, Color4 color) {
    EmitLayer(_particles, at, color, _style.particles.dashCount, DashRecipe(facing, _style.particles));
}
void FxStage::Steam(Float2 at, Color4 color) {
    const auto& tuning = _style.particles;
    const Color4 steam = Mix(color, Color4{255, 255, 255, 255}, tuning.steamWhiten);
    EmitLayer(_particles, at, WithAlpha(steam, tuning.steamOpacity), tuning.steamCount, SteamRecipe(tuning));
}
void FxStage::FloatText(Float2 at, const std::string& text, Color4 color) { _texts.Emit(at, text, color, _style.text); }
void FxStage::PopText(Float2 at, const std::string& text, Color4 color, float size) {
    auto style = _style.text;
    style.text.size = size;
    style.popSeconds = _style.textPopSeconds;
    _texts.Emit(at, text, color, style);
}
void FxStage::Ring(Float2 at, Color4 color) { _rings.Emit(at, color, _style.ring); }
void FxStage::Toast(const std::string& text, Color4 color) { _overlays.Toast(text, color, _style.toast); }
void FxStage::Flash(Color4 color) { _overlays.Flash(color, _style.flash); }
void FxStage::Banner(const std::string& text) { _overlays.Banner(text, _style.banner); }
void FxStage::Shake(float seconds) { _camera.Shake(seconds); }
void FxStage::Punch(float amount, std::optional<Float2> focus) { _camera.Punch(amount, focus); }
void FxStage::Kick(Float2 direction) { _camera.Kick(direction); }
void FxStage::SetMotion(float intensity, Color4 color, Float2 facing) {
    _motionColor = color;
    _camera.SetMotion(intensity, facing);
}
void FxStage::SetPulse(bool active, Color4 color) { _pulse.SetActive(active, color); }
void FxStage::Shockwave(Float2 at, Color4 color, float reach) { _shockwaves.Emit(at, color, reach); }
void FxStage::Glitch(float seconds) { _glitch.Trigger(seconds); }
void FxStage::SlowMotion(float seconds, Color4 color) {
    _slowMotionAge = 0.0f;
    _slowMotionDuration = std::max(0.0f, seconds);
    _slowMotionColor = color;
}
CameraView FxStage::WorldCamera(float timeSeconds) const {
    return _camera.Apply(ShotCamera(_shot, _area, _area), timeSeconds);
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
    if (_slowMotionAge < _slowMotionDuration) {
        const auto& slow = _style.slowMotion;
        const float fadeStart = _slowMotionDuration * slow.fadeStart;
        const float strength = 1.0f - EaseOutCubic(Progress01(_slowMotionAge, fadeStart, _slowMotionDuration));
        paint.Rect(_area, Fill(WithAlpha(_slowMotionColor, slow.tintAlpha * strength)));
        DrawVignette(paint, _viewArea, slow.vignetteSize, WithAlpha(_slowMotionColor, slow.vignetteAlpha * strength));
        DrawSpeedLines(paint, _area, strength, _slowMotionColor, _slowMotionAge, slow.lines);
    }
    _overlays.Draw(paint, _area, _viewArea);
    _glitch.Draw(paint, _area, paint.Time());
}
