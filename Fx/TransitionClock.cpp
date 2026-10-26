#include "Fx/TransitionClock.h"
#include <stdexcept>
#include "Graphics/Anim/Easing.h"

TransitionClock::TransitionClock(float closeSeconds, float holdSeconds, float openSeconds)
    : _closeSeconds(closeSeconds)
    , _holdSeconds(holdSeconds)
    , _openSeconds(openSeconds) {
    if (!(closeSeconds > 0.0f && holdSeconds >= 0.0f && openSeconds > 0.0f)) {
        throw std::invalid_argument("Invalid transition duration");
    }
}
void TransitionClock::Start() { _elapsed = 0.0f; }
bool TransitionClock::Update(float deltaSeconds) {
    if (!_elapsed) return false;
    const float before = *_elapsed;
    *_elapsed += deltaSeconds;
    const bool midpoint = before < _closeSeconds && *_elapsed >= _closeSeconds;
    if (*_elapsed >= _closeSeconds + _holdSeconds + _openSeconds) _elapsed.reset();
    return midpoint;
}
float TransitionClock::Cover(bool& opening) const {
    if (!_elapsed) {
        opening = false;
        return 0.0f;
    }
    const float time = *_elapsed;
    opening = time >= _closeSeconds + _holdSeconds;
    if (time < _closeSeconds) return EaseInOutCubic(time / _closeSeconds);
    if (!opening) return 1.0f;
    return 1.0f - EaseInOutCubic((time - _closeSeconds - _holdSeconds) / _openSeconds);
}
