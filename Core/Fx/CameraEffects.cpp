#include "Fx/CameraEffects.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Animation/Easing.h"

CameraEffects::CameraEffects(const CameraEffectsStyle& settings)
    : _settings(settings) {
    for (float value :
         {settings.shakeAmplitude, settings.shakeFrequency.x, settings.shakeFrequency.y, settings.shakeFadeSeconds,
          settings.punchDecay, settings.kickImpulse, settings.kickStiffness, settings.kickDamping,
          settings.motionBlendSpeed, settings.motionLeadSpeed, settings.motionLeadDistance, settings.motionZoomOut}) {
        if (!std::isfinite(value) || value < 0.0f) throw std::invalid_argument("Invalid camera effects settings");
    }
    if (!std::isfinite(settings.maxStep) || settings.maxStep <= 0.0f || settings.motionZoomOut >= 1.0f) {
        throw std::invalid_argument("Invalid camera step or zoom-out");
    }
}

void CameraEffects::Shake(float seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0f) throw std::invalid_argument("Shake duration must be positive");
    _shakeLife = std::max(_shakeLife, seconds);
}

void CameraEffects::Punch(float amount, std::optional<Float2> focus) {
    if (!std::isfinite(amount) || amount < 0.0f) throw std::invalid_argument("Punch amount must be nonnegative");
    _punch = std::max(_punch, amount);
    _punchFocus = focus;
}

void CameraEffects::Kick(Float2 direction) {
    _kick.velocity.x += direction.x * _settings.kickImpulse;
    _kick.velocity.y += direction.y * _settings.kickImpulse;
}

void CameraEffects::SetMotion(float intensity, Float2 direction) {
    // With motion effects off the camera stays still at speed (and the speed lines that follow it fade).
    _motionTarget = _quality.motion ? Clamp01(intensity) : 0.0f;
    _motionLeadTarget = {direction.x * _motionTarget, direction.y * _motionTarget};
}

void CameraEffects::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    _shakeLife = CountDown(_shakeLife, deltaSeconds);
    _punch *= DecayFactor(_settings.punchDecay, deltaSeconds);
    _kick.Follow({}, deltaSeconds, _settings.kickStiffness, _settings.kickDamping, _settings.maxStep);
    const float blend =
        _settings.motionBlendSpeed > 0.0f ? 1.0f - DecayFactor(_settings.motionBlendSpeed, deltaSeconds) : 1.0f;
    _motion += (_motionTarget - _motion) * blend;
    const float lead =
        _settings.motionLeadSpeed > 0.0f ? 1.0f - DecayFactor(_settings.motionLeadSpeed, deltaSeconds) : 1.0f;
    _motionLead.x += (_motionLeadTarget.x * _settings.motionLeadDistance - _motionLead.x) * lead;
    _motionLead.y += (_motionLeadTarget.y * _settings.motionLeadDistance - _motionLead.y) * lead;
}

void CameraEffects::Clear() {
    _shakeLife = 0.0f;
    _punch = 0.0f;
    _punchFocus.reset();
    _kick = {};
    _motion = 0.0f;
    _motionTarget = 0.0f;
    _motionLead = {};
    _motionLeadTarget = {};
}

CameraView CameraEffects::Apply(const CameraView& base, float timeSeconds) const {
    CameraView camera = base;
    if (_punchFocus) {
        camera.offset = base.ToScreen(*_punchFocus);
        camera.target = *_punchFocus;
    }
    // The player's shake choice scales shake, kick and punch zoom; the motion lead is not affected.
    const float scale = _quality.ShakeScale();
    Float2 shake{};
    if (_shakeLife > 0.0f) {
        const float fade =
            _settings.shakeFadeSeconds > 0.0f ? std::min(_shakeLife / _settings.shakeFadeSeconds, 1.0f) : 1.0f;
        const float amplitude = _settings.shakeAmplitude * fade;
        shake = {amplitude * std::sin(timeSeconds * _settings.shakeFrequency.x),
                 amplitude * std::cos(timeSeconds * _settings.shakeFrequency.y)};
    }
    camera.offset.x += (shake.x + _kick.position.x) * scale - _motionLead.x;
    camera.offset.y += (shake.y + _kick.position.y) * scale - _motionLead.y;
    camera.zoom *= 1.0f + _punch * scale - _settings.motionZoomOut * _motion;
    return camera;
}
