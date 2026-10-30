#include "Fx/CameraEffects2D.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Graphics/Anim/Easing.h"
#include "Graphics/Draw/EffectQuality.h"

CameraEffects2D::CameraEffects2D(const CameraEffectsSettings& settings)
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

void CameraEffects2D::Shake(float seconds) {
    if (!std::isfinite(seconds) || seconds <= 0.0f) throw std::invalid_argument("Shake duration must be positive");
    _shakeLife = std::max(_shakeLife, seconds);
}

void CameraEffects2D::Punch(float amount, std::optional<Vector2> focus) {
    if (!std::isfinite(amount) || amount < 0.0f) throw std::invalid_argument("Punch amount must be nonnegative");
    _punch = std::max(_punch, amount);
    _punchFocus = focus;
}

void CameraEffects2D::Kick(Vector2 direction) {
    _kickVelocity.x += direction.x * _settings.kickImpulse;
    _kickVelocity.y += direction.y * _settings.kickImpulse;
}

void CameraEffects2D::SetMotion(float intensity, Vector2 direction) {
    // Sprint efektleri kapalıyken kamera hareketi (ve ona bağlı hız çizgileri) söner.
    _motionTarget = CurrentEffectQuality().sprintEffects ? Clamp01(intensity) : 0.0f;
    _motionLeadTarget = {direction.x * _motionTarget, direction.y * _motionTarget};
}

void CameraEffects2D::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    _shakeLife = CountDown(_shakeLife, deltaSeconds);
    const float step = std::min(deltaSeconds, _settings.maxStep);
    _punch -= _punch * std::min(1.0f, _settings.punchDecay * step);
    _kickVelocity.x += (-_settings.kickStiffness * _kickOffset.x - _settings.kickDamping * _kickVelocity.x) * step;
    _kickVelocity.y += (-_settings.kickStiffness * _kickOffset.y - _settings.kickDamping * _kickVelocity.y) * step;
    _kickOffset.x += _kickVelocity.x * step;
    _kickOffset.y += _kickVelocity.y * step;
    const float blend =
        _settings.motionBlendSpeed > 0.0f ? std::min(1.0f, _settings.motionBlendSpeed * deltaSeconds) : 1.0f;
    _motion += (_motionTarget - _motion) * blend;
    const float lead =
        _settings.motionLeadSpeed > 0.0f ? std::min(1.0f, _settings.motionLeadSpeed * deltaSeconds) : 1.0f;
    _motionLead.x += (_motionLeadTarget.x * _settings.motionLeadDistance - _motionLead.x) * lead;
    _motionLead.y += (_motionLeadTarget.y * _settings.motionLeadDistance - _motionLead.y) * lead;
}

void CameraEffects2D::Clear() {
    _shakeLife = 0.0f;
    _punch = 0.0f;
    _punchFocus.reset();
    _kickOffset = {};
    _kickVelocity = {};
    _motion = 0.0f;
    _motionTarget = 0.0f;
    _motionLead = {};
    _motionLeadTarget = {};
}

Camera2D CameraEffects2D::Apply(const Camera2D& baseCamera, float timeSeconds) const {
    Camera2D camera = baseCamera;
    if (_punchFocus) {
        camera.offset = GetWorldToScreen2D(*_punchFocus, baseCamera);
        camera.target = *_punchFocus;
    }
    // Oyuncunun sarsıntı tercihi sarsıntıyı, tekmeyi ve zoom vuruşunu ölçekler; hareket önü (sprint) etkilenmez.
    const float scale = ShakeScale();
    Vector2 shake{};
    if (_shakeLife > 0.0f) {
        const float fade =
            _settings.shakeFadeSeconds > 0.0f ? std::min(_shakeLife / _settings.shakeFadeSeconds, 1.0f) : 1.0f;
        const float amplitude = _settings.shakeAmplitude * fade;
        shake = {amplitude * std::sin(timeSeconds * _settings.shakeFrequency.x),
                 amplitude * std::cos(timeSeconds * _settings.shakeFrequency.y)};
    }
    camera.offset.x += (shake.x + _kickOffset.x) * scale - _motionLead.x;
    camera.offset.y += (shake.y + _kickOffset.y) * scale - _motionLead.y;
    camera.zoom *= 1.0f + _punch * scale - _settings.motionZoomOut * _motion;
    return camera;
}
