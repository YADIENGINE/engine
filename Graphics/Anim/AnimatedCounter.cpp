#include "Graphics/Anim/AnimatedCounter.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Anim/Easing.h"

AnimatedCounter::AnimatedCounter(const AnimatedCounterSettings& settings)
    : _settings(settings) {}
void AnimatedCounter::Clear() {
    _shown = 0.0;
    _target = 0;
    _punch = 0.0f;
}
void AnimatedCounter::Update(int target, float deltaSeconds) {
    if (target > _target)
        _punch = 1.0f;
    else if (target < _target)
        _shown = static_cast<double>(target);
    _target = target;
    const float step = _settings.speed > 0.0f ? std::min(1.0f, _settings.speed * deltaSeconds) : 1.0f;
    _shown += (static_cast<double>(target) - _shown) * step;
    if (std::fabs(static_cast<double>(target) - _shown) < _settings.snapDistance) _shown = static_cast<double>(target);
    _punch = _settings.punchSeconds > 0.0f ? CountDown(_punch, deltaSeconds / _settings.punchSeconds) : 0.0f;
}
