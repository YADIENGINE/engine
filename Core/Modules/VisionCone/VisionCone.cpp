#include "Modules/VisionCone/VisionCone.h"
#include <stdexcept>
#include "Modules/VisionCone/Internals/ConeGeometry.h"
#include "Modules/VisionCone/Internals/SightQuery.h"

bool VisionCone::Contains(const VisionConeSpec& spec, Float2 point) {
    return ConeGeometry(spec).Classify(point).status == VisionStatus::Visible;
}

VisionPointResult VisionCone::Query(const VisionConeSpec& spec, Float2 point, const VisionTrace& trace) {
    return SightQuery::Evaluate(ConeGeometry(spec), spec.origin, point, trace);
}

VisionSampleResult VisionCone::QuerySamples(const VisionConeSpec& spec, std::span<const Float2> points,
                                            const VisionTrace& trace) {
    const ConeGeometry geometry(spec);
    VisionSampleResult counts{.total = points.size()};
    for (Float2 point : points) {
        const auto result = SightQuery::Evaluate(geometry, spec.origin, point, trace);
        if (result.status == VisionStatus::Visible || result.status == VisionStatus::Occluded) ++counts.inCone;
        if (result.status == VisionStatus::Visible) ++counts.visible;
    }
    return counts;
}

VisionTrace VisionCone::CollisionTrace(const CollisionWorld& world, CollisionMask blockers, ColliderId ignore) {
    return [&world, blockers, ignore](Float2 from, Float2 to) -> std::optional<float> {
        if (const auto hit = world.Raycast(from, to, blockers, ignore)) return hit->fraction;
        return std::nullopt;
    };
}

std::span<const Float2> VisionCone::Outline(const VisionConeSpec& spec, const VisionOutlineOptions& options,
                                            const VisionTrace& trace) {
    _outline.clear();
    const ConeGeometry geometry(spec);
    if (options.segments < (geometry.FullCircle() ? 3 : 1))
        throw std::invalid_argument("VisionCone: too few outline segments");
    if (spec.range == 0.0f) return _outline;
    const auto segments = static_cast<std::size_t>(options.segments);
    const std::size_t samples = segments + (geometry.FullCircle() ? 0U : 1U);
    _outline.reserve(samples + (geometry.FullCircle() ? 0U : 1U));
    if (!geometry.FullCircle()) _outline.push_back(spec.origin);
    for (std::size_t i = 0; i < samples; ++i) {
        Float2 point = geometry.Endpoint(static_cast<double>(i) / static_cast<double>(segments));
        if (const auto fraction = SightQuery::Trace(trace, spec.origin, point)) {
            // Double intermediates avoid overflow when finite endpoints lie on opposite sides of the float range.
            point = {static_cast<float>(spec.origin.x + (static_cast<double>(point.x) - spec.origin.x) * *fraction),
                     static_cast<float>(spec.origin.y + (static_cast<double>(point.y) - spec.origin.y) * *fraction)};
        }
        if (_outline.empty() || _outline.back() != point) _outline.push_back(point);
    }
    if (_outline.size() > 1 && _outline.front() == _outline.back()) _outline.pop_back();
    if (_outline.size() < 3) _outline.clear();
    return _outline;
}
