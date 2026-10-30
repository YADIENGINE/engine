#pragma once
#include <raylib.h>
#include <optional>

struct CameraEffectsSettings {
    float shakeAmplitude = 0.0f;
    Vector2 shakeFrequency{}; // radians/second
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
    void Shake(float seconds);
    void Punch(float amount, std::optional<Vector2> focus = std::nullopt);
    void Kick(Vector2 direction);
    void SetMotion(float intensity, Vector2 direction);
    void Update(float deltaSeconds);
    void Clear();
    // Focus is a world position; offsets/directions are in canvas pixels/axes. The clock belongs to the caller.
    [[nodiscard]] Camera2D Apply(const Camera2D& baseCamera, float timeSeconds) const;
    [[nodiscard]] float MotionIntensity() const { return _motion; }

private:
    CameraEffectsSettings _settings;
    float _shakeLife = 0.0f;
    float _punch = 0.0f;
    std::optional<Vector2> _punchFocus;
    Vector2 _kickOffset{};
    Vector2 _kickVelocity{};
    float _motion = 0.0f;
    float _motionTarget = 0.0f;
    Vector2 _motionLead{};
    Vector2 _motionLeadTarget{};
};
