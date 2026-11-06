#include "Fx/Camera/CameraEffects.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Animation/Easing.h"
#include "Fx/Internals/FxChecks.h"
#include "Utilities/Math/NoiseField.h"

bool IsValid(const CameraEffectsStyle& style) {
    for (const float value :
         {style.shakeOffset, style.shakeAngle, style.shakeSpeed, style.shakeDecay, style.punchDecay,
          style.kickImpulse, style.kickStiffness, style.kickDamping, style.motionBlendSpeed, style.motionLeadSpeed,
          style.motionLeadDistance, style.motionZoomOut}) {
        if (!FxNotNegative(value)) return false;
    }
    const bool shakes = style.shakeOffset > 0.0f || style.shakeAngle > 0.0f;
    return FxPositive(style.maxStep) && style.motionZoomOut < 1.0f && (!shakes || style.shakeDecay > 0.0f);
}

CameraEffects::CameraEffects(const CameraEffectsStyle& settings)
    : _settings(settings) {
    if (!IsValid(settings)) throw std::invalid_argument("Invalid camera effects style");
}

void CameraEffects::Shake(float amount) {
    if (!FxNotNegative(amount)) throw std::invalid_argument("Shake amount must be nonnegative");
    _shake = std::min(_shake + amount, 1.0f);
}

void CameraEffects::Punch(float amount, std::optional<Float2> focus) {
    if (!std::isfinite(amount) || amount < 0.0f) throw std::invalid_argument("Punch amount must be nonnegative");
    _punch = std::max(_punch, amount);
    _punchFocus = focus;
}

void CameraEffects::Kick(Float2 direction) {
    if (!FxFinite(direction)) throw std::invalid_argument("Kick direction must be finite");
    _kick.velocity.x += direction.x * _settings.kickImpulse;
    _kick.velocity.y += direction.y * _settings.kickImpulse;
}

void CameraEffects::SetMotion(float intensity, Float2 direction) {
    if (!std::isfinite(intensity) || !FxFinite(direction)) throw std::invalid_argument("Motion values must be finite");
    // With motion effects off the camera stays still at speed (and the speed lines that follow it fade).
    _motionTarget = _quality.motion ? Clamp01(intensity) : 0.0f;
    _motionLeadTarget = {direction.x * _motionTarget, direction.y * _motionTarget};
}

void CameraEffects::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    _shake = std::max(_shake - _settings.shakeDecay * deltaSeconds, 0.0f);
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
    _shake = 0.0f;
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
    if (_shake > 0.0f) {
        const float strength = _shake * _shake;
        const float noiseAt = timeSeconds * _settings.shakeSpeed;
        shake = {_settings.shakeOffset * strength * Noise(NoiseSettings{.seed = 1U}, noiseAt),
                 _settings.shakeOffset * strength * Noise(NoiseSettings{.seed = 2U}, noiseAt)};
        camera.rotation += _settings.shakeAngle * strength * Noise(NoiseSettings{.seed = 3U}, noiseAt) * scale;
    }
    camera.offset.x += (shake.x + _kick.position.x) * scale - _motionLead.x;
    camera.offset.y += (shake.y + _kick.position.y) * scale - _motionLead.y;
    camera.zoom *= 1.0f + _punch * scale - _settings.motionZoomOut * _motion;
    return camera;
}
