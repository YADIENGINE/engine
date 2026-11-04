#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Animation/Easing.h"

struct ButtonAnimSettings {
    float highlightStiffness = 0.0f; // zero selects immediately
    float highlightDamping = 0.0f;
    float maxStep = 1.0f / 30.0f;
    float pressSeconds = 0.0f; // zero disables feedback
    float denySeconds = 0.0f;
};

struct ButtonAnimState {
    float highlight = 0.0f;
    float press = 0.0f;
    float shake = 0.0f;
};

// A button's highlight, press and refusal as values; drawing them is the theme's job. For views, on seconds.
// Header-only.
class ButtonAnim {
public:
    explicit ButtonAnim(const ButtonAnimSettings& settings = {})
        : _settings(settings) {
        for (float value :
             {settings.highlightStiffness, settings.highlightDamping, settings.pressSeconds, settings.denySeconds}) {
            if (!std::isfinite(value) || value < 0.0f) throw std::invalid_argument("Invalid button animation settings");
        }
        if (!std::isfinite(settings.maxStep) || settings.maxStep <= 0.0f) {
            throw std::invalid_argument("Button animation step must be positive");
        }
    }
    void Update(bool selected, float deltaSeconds) {
        if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
        const float step = std::min(deltaSeconds, _settings.maxStep);
        const float target = selected ? 1.0f : 0.0f;
        if (_settings.highlightStiffness > 0.0f) {
            _velocity +=
                ((target - _highlight) * _settings.highlightStiffness - _velocity * _settings.highlightDamping) * step;
            _highlight += _velocity * step;
        } else {
            _highlight = target;
        }
        if (_settings.pressSeconds > 0.0f) _press = CountDown(_press, deltaSeconds / _settings.pressSeconds);
        if (_settings.denySeconds > 0.0f) _shake = CountDown(_shake, deltaSeconds / _settings.denySeconds);
    }
    void Press() { _press = _settings.pressSeconds > 0.0f ? 1.0f : 0.0f; }
    void Deny() { _shake = _settings.denySeconds > 0.0f ? 1.0f : 0.0f; }
    void Clear() {
        _highlight = 0.0f;
        _velocity = 0.0f;
        _press = 0.0f;
        _shake = 0.0f;
    }
    [[nodiscard]] ButtonAnimState State() const {
        return {.highlight = _highlight, .press = EaseOutCubic(_press), .shake = _shake};
    }

private:
    ButtonAnimSettings _settings;
    float _highlight = 0.0f;
    float _velocity = 0.0f;
    float _press = 0.0f;
    float _shake = 0.0f;
};
