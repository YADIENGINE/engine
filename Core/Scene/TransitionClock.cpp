#include "Scene/TransitionClock.h"
#include <stdexcept>

namespace {
SecondsTrack<float> CoverTrack(float closeSeconds, float holdSeconds, float openSeconds) {
    if (!(closeSeconds > 0.0f && holdSeconds >= 0.0f && openSeconds > 0.0f)) {
        throw std::invalid_argument("Invalid transition duration");
    }
    return {{0.0f, 0.0f},
            {closeSeconds, 1.0f, Curve::InOutCubic},
            {closeSeconds + holdSeconds, 1.0f},
            {closeSeconds + holdSeconds + openSeconds, 0.0f, Curve::InOutCubic}};
}
} // namespace

TransitionClock::TransitionClock(float closeSeconds, float holdSeconds, float openSeconds)
    : _closeSeconds(closeSeconds)
    , _openingAt(closeSeconds + holdSeconds)
    , _cover(CoverTrack(closeSeconds, holdSeconds, openSeconds)) {}
void TransitionClock::Start() { _elapsed = 0.0f; }
bool TransitionClock::Update(float deltaSeconds) {
    if (!_elapsed) return false;
    const float before = *_elapsed;
    *_elapsed += deltaSeconds;
    const bool midpoint = before < _closeSeconds && *_elapsed >= _closeSeconds;
    if (*_elapsed >= _cover.Length()) _elapsed.reset();
    return midpoint;
}
float TransitionClock::Cover() const { return _elapsed ? _cover.At(*_elapsed) : 0.0f; }
bool TransitionClock::Opening() const { return _elapsed && *_elapsed >= _openingAt; }
