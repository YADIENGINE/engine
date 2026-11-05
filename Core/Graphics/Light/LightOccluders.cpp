#include "Graphics/Light/LightOccluders.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
bool Inside(std::span<const VisibilityEdge> outline, Float2 point) {
    bool inside = false;
    for (const VisibilityEdge& edge : outline) {
        if ((edge.a.y > point.y) == (edge.b.y > point.y)) continue;
        const float crossing = edge.a.x + (point.y - edge.a.y) / (edge.b.y - edge.a.y) * (edge.b.x - edge.a.x);
        if (point.x < crossing) inside = !inside;
    }
    return inside;
}
} // namespace

void LightOccluders::AddPart(std::span<const Float2> points, const Affine2& at, bool closed, bool selfLit) {
    _placed.clear();
    for (const Float2 point : points) {
        const Float2 placed = at.Apply(point);
        if (!std::isfinite(placed.x) || !std::isfinite(placed.y))
            throw std::invalid_argument("LightOccluders: a point is not finite");
        _placed.push_back(placed);
    }
    if (closed) {
        // Always the same direction, so that the side of an edge says where its inside is. A mirroring 'at' reverses
        // it. Measured from its first point, or a small shape far from the origin rounds away.
        float area = 0.0f;
        for (std::size_t i = 1; i + 1 < _placed.size(); ++i)
            area += Cross(_placed[i] - _placed[0], _placed[i + 1] - _placed[0]);
        if (area < 0.0f) std::reverse(_placed.begin(), _placed.end());
    }
    Part part{.first = _edges.size(), .low = _placed[0], .high = _placed[0], .closed = closed, .selfLit = selfLit};
    const std::size_t edges = closed ? _placed.size() : _placed.size() - 1;
    for (std::size_t i = 0; i < edges; ++i) {
        const Float2 point = _placed[i];
        _edges.push_back({point, _placed[(i + 1) % _placed.size()]});
    }
    for (const Float2 point : _placed) {
        part.low = {std::min(part.low.x, point.x), std::min(part.low.y, point.y)};
        part.high = {std::max(part.high.x, point.x), std::max(part.high.y, point.y)};
    }
    part.count = edges;
    _parts.push_back(part);
}

void LightOccluders::Outline(std::span<const Float2> closed, const Affine2& at, bool selfLit) {
    if (closed.size() < 3) throw std::invalid_argument("LightOccluders: an outline needs 3 points");
    AddPart(closed, at, true, selfLit);
}

void LightOccluders::Line(std::span<const Float2> open, const Affine2& at) {
    if (open.size() < 2) throw std::invalid_argument("LightOccluders: a line needs 2 points");
    AddPart(open, at, false, false);
}

void LightOccluders::Clear() {
    _edges.clear();
    _parts.clear();
}

void LightOccluders::AppendBlocking(Float2 from, float radius, std::vector<VisibilityEdge>& edges) const {
    for (const Part& part : _parts) {
        if (part.high.x < from.x - radius || part.low.x > from.x + radius || part.high.y < from.y - radius ||
            part.low.y > from.y + radius)
            continue;
        const std::span<const VisibilityEdge> own(_edges.data() + part.first, part.count);
        if (part.closed && Inside(own, from)) continue;
        for (const VisibilityEdge& edge : own) {
            // The inside lies on the positive side of every edge of an outline: the light is behind this one's face.
            if (part.selfLit && Cross(edge.b - edge.a, from - edge.a) < 0.0f) continue;
            edges.push_back(edge);
        }
    }
}
