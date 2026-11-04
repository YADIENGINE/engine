#include "Runtime/TimeScale.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

void TimeScale::Hitstop(float seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0f) throw std::invalid_argument("Invalid hitstop duration");
    _hitstopSeconds = std::max(_hitstopSeconds, seconds);
}

void TimeScale::SlowMo(float seconds, float startScale, float endScale, Curve curve) {
    if (!std::isfinite(seconds) || seconds < 0.0f || !std::isfinite(startScale) || startScale < 0.0f ||
        !std::isfinite(endScale) || endScale < 0.0f) {
        throw std::invalid_argument("Invalid slow motion");
    }
    _slowMo.Snap(startScale);
    if (seconds > 0.0f) _slowMo.To(endScale, seconds, curve);
}

void TimeScale::Clear() {
    _hitstopSeconds = 0.0f;
    _slowMo.Stop();
}

float TimeScale::Advance(float realSeconds) {
    if (_hitstopSeconds > 0.0f) {
        _hitstopSeconds -= realSeconds;
        return 0.0f;
    }
    if (!_slowMo.Active()) return realSeconds;
    const float scale = _slowMo.Value(); // Where this frame starts
    _slowMo.Update(realSeconds);
    return realSeconds * scale;
}
