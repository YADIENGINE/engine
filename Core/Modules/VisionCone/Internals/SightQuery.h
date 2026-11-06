#pragma once
#include "Modules/VisionCone/Internals/ConeGeometry.h"

// Shared synchronous trace validation and narrow-phase query, never exposed to consumers.
class SightQuery {
public:
    [[nodiscard]] static std::optional<float> Trace(const VisionTrace& trace, Float2 from, Float2 to);
    [[nodiscard]] static VisionPointResult Evaluate(const ConeGeometry& geometry, Float2 from, Float2 point,
                                                    const VisionTrace& trace);
};
