#include "Camera/CameraSettings.h"
#include <cmath>

namespace {
bool NonNegative(float value) { return std::isfinite(value) && value >= 0.0f; }
bool Positive(float value) { return std::isfinite(value) && value > 0.0f; }
bool Fraction(float value) { return NonNegative(value) && value <= 1.0f; }
bool Finite(Float2 value) { return std::isfinite(value.x) && std::isfinite(value.y); }
} // namespace

bool IsValid(const CameraSettings& settings) {
    const auto& follow = settings.follow;
    const auto& zoom = settings.zoom;
    const bool followValid = Fraction(follow.deadZone.x) && Fraction(follow.deadZone.y) &&
                             NonNegative(follow.smoothTime) && NonNegative(follow.lookAhead.x) &&
                             NonNegative(follow.lookAhead.y) && Fraction(follow.lookAheadLimit) &&
                             NonNegative(follow.lookAheadSmoothTime) && Finite(follow.offset);
    const bool zoomValid = Positive(zoom.zoom) && Positive(zoom.min) && Positive(zoom.max) && zoom.min <= zoom.max &&
                           NonNegative(zoom.padding) && NonNegative(zoom.smoothTime);
    const bool limitsValid = !settings.limits ||
                             (Finite(settings.limits->Position()) && Finite(settings.limits->Size()) &&
                              !settings.limits->Empty());
    return followValid && zoomValid && limitsValid;
}
