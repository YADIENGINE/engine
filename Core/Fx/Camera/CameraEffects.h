#pragma once
#include <optional>
#include "Animation/SpringPoint.h"
#include "Fx/FxQuality.h"
#include "Utilities/CameraView.h"
#include "Utilities/Float2.h"

struct CameraEffectsStyle {
    float shakeOffset = 0.0f; // Canvas units at full shake
    float shakeAngle = 0.0f;  // Radians at full shake
    float shakeSpeed = 0.0f;  // How fast it jitters: new directions per second
    float shakeDecay = 0.0f;  // Shake amount lost per second; needed when there is a shake
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

[[nodiscard]] bool IsValid(const CameraEffectsStyle& style);

class CameraEffects {
public:
    explicit CameraEffects(const CameraEffectsStyle& settings);
    // Shake scales shake, punch and kick; motion off keeps the camera still at speed.
    void SetQuality(const FxQuality& quality) { _quality = quality; }
    // Adds to the shake, up to 1: 0.3 a light hit, 0.6 a heavy one, 1 the most. What shows is the square of the amount,
    // so small hits stay small and hits in a row build up. It fades at shakeDecay, whatever the frame rate.
    void Shake(float amount);
    void Punch(float amount, std::optional<Float2> focus = std::nullopt);
    void Kick(Float2 direction);
    void SetMotion(float intensity, Float2 direction);
    void Update(float deltaSeconds);
    void Clear();
    // Focus is a world position; offsets/directions are in canvas units/axes. The clock belongs to the caller.
    [[nodiscard]] CameraView Apply(const CameraView& base, float timeSeconds) const;
    [[nodiscard]] float MotionIntensity() const { return _motion; }
    [[nodiscard]] float ShakeAmount() const { return _shake; }

private:
    CameraEffectsStyle _settings;
    FxQuality _quality;
    float _shake = 0.0f;
    float _punch = 0.0f;
    std::optional<Float2> _punchFocus;
    SpringPoint _kick;
    float _motion = 0.0f;
    float _motionTarget = 0.0f;
    Float2 _motionLead{};
    Float2 _motionLeadTarget{};
};
