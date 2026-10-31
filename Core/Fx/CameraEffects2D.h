#pragma once
#include <optional>
#include "Fx/FxQuality.h"
#include "Utilities/CameraView.h"
#include "Utilities/Float2.h"

struct CameraEffectsSettings {
    float shakeAmplitude = 0.0f;
    Float2 shakeFrequency{}; // radians/second
    float shakeFadeSeconds = 0.0f;
    float punchDecay = 0.0f;
    float kickImpulse = 0.0f;
    float kickStiffness = 0.0f;
    float kickDamping = 0.0f;
    float maxStep = 1.0f / 30.0f;
    float motionBlendSpeed = 0.0f;
    float motionLeadSpeed = 0.0f;
    float motionLeadDistance = 0.0f;
    float motionZoomOut = 0.0f;
};

class CameraEffects2D {
public:
    explicit CameraEffects2D(const CameraEffectsSettings& settings);
    // Shake scales shake, punch and kick; motion off keeps the camera still at speed.
    void SetQuality(const FxQuality& quality) { _quality = quality; }
    void Shake(float seconds);
    void Punch(float amount, std::optional<Float2> focus = std::nullopt);
    void Kick(Float2 direction);
    void SetMotion(float intensity, Float2 direction);
    void Update(float deltaSeconds);
    void Clear();
    // Focus is a world position; offsets/directions are in canvas units/axes. The clock belongs to the caller.
    [[nodiscard]] CameraView Apply(const CameraView& base, float timeSeconds) const;
    [[nodiscard]] float MotionIntensity() const { return _motion; }

private:
    CameraEffectsSettings _settings;
    FxQuality _quality;
    float _shakeLife = 0.0f;
    float _punch = 0.0f;
    std::optional<Float2> _punchFocus;
    Float2 _kickOffset{};
    Float2 _kickVelocity{};
    float _motion = 0.0f;
    float _motionTarget = 0.0f;
    Float2 _motionLead{};
    Float2 _motionLeadTarget{};
};
