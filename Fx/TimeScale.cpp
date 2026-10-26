#include "Fx/TimeScale.h"
#include <algorithm>

void TimeScale::Hitstop(float seconds) { _hitstopSeconds = std::max(_hitstopSeconds, seconds); }

void TimeScale::SlowMo(float seconds, float startScale, float endScale) {
    _slowMoSeconds = seconds;
    _slowMoDuration = seconds;
    _slowMoStartScale = startScale;
    _slowMoEndScale = endScale;
}

void TimeScale::Clear() {
    _hitstopSeconds = 0.0f;
    _slowMoSeconds = 0.0f;
}

float TimeScale::Advance(float realSeconds) {
    if (_hitstopSeconds > 0.0f) {
        _hitstopSeconds -= realSeconds;
        return 0.0f;
    }
    if (_slowMoSeconds <= 0.0f) {
        return realSeconds;
    }
    const float t = 1.0f - _slowMoSeconds / _slowMoDuration; // 0: başı, 1: sonu
    _slowMoSeconds = std::max(0.0f, _slowMoSeconds - realSeconds);
    return realSeconds * (_slowMoStartScale + (_slowMoEndScale - _slowMoStartScale) * t);
}
