#include "Graphics/Anim/ButtonAnim.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Graphics/Anim/Easing.h"

ButtonAnim::ButtonAnim(const ButtonAnimSettings& settings)
    : _settings(settings) {
    for (float value :
         {settings.highlightStiffness, settings.highlightDamping, settings.pressSeconds, settings.denySeconds}) {
        if (!std::isfinite(value) || value < 0.0f) throw std::invalid_argument("Invalid button animation settings");
    }
    if (!std::isfinite(settings.maxStep) || settings.maxStep <= 0.0f) {
        throw std::invalid_argument("Button animation step must be positive");
    }
}
void ButtonAnim::Update(bool selected, float deltaSeconds) {
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
void ButtonAnim::Press() { _press = _settings.pressSeconds > 0.0f ? 1.0f : 0.0f; }
void ButtonAnim::Deny() { _shake = _settings.denySeconds > 0.0f ? 1.0f : 0.0f; }
void ButtonAnim::Clear() {
    _highlight = 0.0f;
    _velocity = 0.0f;
    _press = 0.0f;
    _shake = 0.0f;
}
ButtonAnimState ButtonAnim::State() const {
    return {.highlight = _highlight, .press = EaseOutCubic(_press), .shake = _shake};
}
