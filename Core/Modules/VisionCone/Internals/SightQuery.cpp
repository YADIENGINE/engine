#include "Modules/VisionCone/Internals/SightQuery.h"
#include <cmath>
#include <stdexcept>

std::optional<float> SightQuery::Trace(const VisionTrace& trace, Float2 from, Float2 to) {
    if (!trace) return std::nullopt;
    const auto fraction = trace(from, to);
    if (fraction && (!std::isfinite(*fraction) || *fraction < 0.0f || *fraction > 1.0f))
        throw std::invalid_argument("VisionCone: trace fraction must be finite and in [0, 1]");
    return fraction;
}

VisionPointResult SightQuery::Evaluate(const ConeGeometry& geometry, Float2 from, Float2 point,
                                       const VisionTrace& trace) {
    auto result = geometry.Classify(point);
    if (result.status != VisionStatus::Visible) return result;
    result.blockingFraction = Trace(trace, from, point);
    if (result.blockingFraction) result.status = VisionStatus::Occluded;
    return result;
}
