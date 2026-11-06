#pragma once
#include "Modules/VisionCone/VisionCone.h"

// Validated, normalized geometry shared by queries and contour sampling. Owner-private.
class ConeGeometry {
public:
    explicit ConeGeometry(const VisionConeSpec& spec);
    [[nodiscard]] VisionPointResult Classify(Float2 point) const;
    [[nodiscard]] Float2 Endpoint(double arcFraction) const;
    [[nodiscard]] bool FullCircle() const { return _fullCircle; }

private:
    Float2 _origin;
    double _direction;
    double _range;
    double _halfAngle;
    bool _fullCircle;
};
