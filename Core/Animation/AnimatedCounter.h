#pragma once
#include <algorithm>
#include <cmath>
#include "Animation/Easing.h"

struct AnimatedCounterSettings {
    float speed = 0.0f;
    float punchSeconds = 0.0f;
    double snapDistance = 0.5;
};

// A number on screen that runs up to its target and punches when it grows. For views, on seconds. Header-only.
class AnimatedCounter {
public:
    explicit AnimatedCounter(const AnimatedCounterSettings& settings = {})
        : _settings(settings) {}
    void Update(int target, float deltaSeconds) {
        if (target > _target)
            _punch = 1.0f;
        else if (target < _target)
            _shown = static_cast<double>(target);
        _target = target;
        const float step = _settings.speed > 0.0f ? std::min(1.0f, _settings.speed * deltaSeconds) : 1.0f;
        _shown += (static_cast<double>(target) - _shown) * step;
        if (std::fabs(static_cast<double>(target) - _shown) < _settings.snapDistance)
            _shown = static_cast<double>(target);
        _punch = _settings.punchSeconds > 0.0f ? CountDown(_punch, deltaSeconds / _settings.punchSeconds) : 0.0f;
    }
    void Clear() {
        _shown = 0.0;
        _target = 0;
        _punch = 0.0f;
    }
    [[nodiscard]] double Shown() const { return _shown; }
    [[nodiscard]] int Target() const { return _target; }
    [[nodiscard]] float Punch() const { return _punch; }

private:
    AnimatedCounterSettings _settings;
    double _shown = 0.0;
    int _target = 0;
    float _punch = 0.0f;
};
