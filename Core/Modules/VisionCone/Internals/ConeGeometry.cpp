#include "Modules/VisionCone/Internals/ConeGeometry.h"
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {
constexpr double Turn = 2.0 * std::numbers::pi;
// Float coordinates and directions can round an exact boundary to either side by a few ulps.
constexpr double AngularTolerance = 4.0 * std::numeric_limits<float>::epsilon();

void ValidatePoint(Float2 point) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y))
        throw std::invalid_argument("VisionCone: point must be finite");
}
} // namespace

ConeGeometry::ConeGeometry(const VisionConeSpec& spec)
    : _origin(spec.origin)
    , _direction(spec.direction)
    , _range(spec.range)
    , _halfAngle(spec.halfAngle)
    , _fullCircle(spec.halfAngle == Pi) {
    ValidatePoint(_origin);
    if (!std::isfinite(_direction) || !std::isfinite(_range) || _range < 0.0 || !std::isfinite(_halfAngle) ||
        _halfAngle <= 0.0 || _halfAngle > Pi)
        throw std::invalid_argument("VisionCone: invalid direction, range or halfAngle");
    _direction = std::remainder(_direction, Turn);
}

VisionPointResult ConeGeometry::Classify(Float2 point) const {
    ValidatePoint(point);
    const double dx = static_cast<double>(point.x) - _origin.x;
    const double dy = static_cast<double>(point.y) - _origin.y;
    const double distance = std::hypot(dx, dy);
    if (_range == 0.0 || distance > _range) return {VisionStatus::OutOfRange, distance, std::nullopt};
    if (!_fullCircle && distance > 0.0) {
        const double angle = std::abs(std::remainder(std::atan2(dy, dx) - _direction, Turn));
        if (angle > _halfAngle + AngularTolerance) return {VisionStatus::OutsideAngle, distance, std::nullopt};
    }
    return {VisionStatus::Visible, distance, std::nullopt};
}

Float2 ConeGeometry::Endpoint(double arcFraction) const {
    const double half = _fullCircle ? std::numbers::pi : _halfAngle;
    const double angle = _direction - half + 2.0 * half * arcFraction;
    const double x = _origin.x + std::cos(angle) * _range;
    const double y = _origin.y + std::sin(angle) * _range;
    const double limit = std::numeric_limits<float>::max();
    if (std::abs(x) > limit || std::abs(y) > limit)
        throw std::invalid_argument("VisionCone: outline endpoint is not representable");
    return {static_cast<float>(x), static_cast<float>(y)};
}
