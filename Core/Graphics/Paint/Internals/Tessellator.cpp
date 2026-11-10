#include "Graphics/Paint/Internals/Tessellator.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include "Graphics/Paint/Internals/PathAccess.h"
#include "Utilities/Math/Scalar.h"

namespace {
constexpr float SamePoint = 1e-4f;
constexpr float RoundTolerance = 0.25f; // Of the fringe width: how far round joins / caps may deviate from a circle
constexpr int MaxRoundSteps = 32;
constexpr float MaxFillMiter = 4.0f; // Polygon corner offsets beyond this many edge widths are clamped
constexpr int MaxCurveSteps = 256;
constexpr PaintColor Clear{};

PaintVertex Vertex(Float2 position, PaintColor color) {
    PaintVertex vertex;
    vertex.x = position.x;
    vertex.y = position.y;
    vertex.color = color;
    return vertex;
}

PaintColor Scaled(PaintColor color, float scale) {
    const auto channel = [scale](std::uint8_t value) {
        return static_cast<std::uint8_t>(std::lround(static_cast<float>(value) * scale));
    };
    return {channel(color.r), channel(color.g), channel(color.b), channel(color.a)};
}

void Quad(MeshBuffer& out, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    out.indices.insert(out.indices.end(), {a, b, c, a, c, d});
}

// Steps for a round join / cap of 'angle' radians at 'radius' with the given fringe width.
int RoundSteps(float angle, float radius, float aa) {
    const float tolerance = RoundTolerance * aa;
    if (radius <= tolerance) return 2;
    const float step = 2.0f * std::acos(std::max(-1.0f, 1.0f - tolerance / radius));
    return std::clamp(static_cast<int>(std::ceil(std::abs(angle) / std::max(step, 1e-3f))), 2, MaxRoundSteps);
}

// A cross-section of a stroke: centre plus the offset directions (per unit of width) of its two sides.
struct Row {
    Float2 point;
    Float2 left;
    Float2 right;
    bool faded = false; // End fringe: every vertex transparent
};

class Ribbon {
public:
    Ribbon(MeshBuffer& out, float core, float outer, PaintColor color)
        : _out(out)
        , _core(core)
        , _outer(outer)
        , _color(color) {}

    // Appends a cross-section; connects it to the previous one.
    void Add(const Row& row) {
        const auto base = static_cast<std::uint32_t>(_out.vertices.size());
        const PaintColor inner = row.faded ? Clear : _color;
        _out.vertices.push_back(Vertex(row.point + row.left * _outer, Clear));
        _out.vertices.push_back(Vertex(row.point + row.left * _core, inner));
        _out.vertices.push_back(Vertex(row.point + row.right * _core, inner));
        _out.vertices.push_back(Vertex(row.point + row.right * _outer, Clear));
        if (_rows > 0) Connect(base - 4, base);
        if (_rows == 0) _first = base;
        ++_rows;
    }
    // Connects the last cross-section to the first (closed strokes).
    void Close() {
        if (_rows > 2) Connect(static_cast<std::uint32_t>(_out.vertices.size()) - 4, _first);
    }

private:
    void Connect(std::uint32_t from, std::uint32_t to) {
        for (std::uint32_t side = 0; side < 3; ++side)
            Quad(_out, from + side, from + side + 1, to + side + 1, to + side);
    }
    MeshBuffer& _out;
    float _core;
    float _outer;
    PaintColor _color;
    std::uint32_t _first = 0;
    int _rows = 0;
};

// Half disc at 'center' bulging towards 'direction', from the +normal side to the -normal side.
void RoundCap(MeshBuffer& out, Float2 center, Float2 direction, float core, float outer, float aa, PaintColor color) {
    const Float2 normal = Perpendicular(direction);
    const int steps = RoundSteps(Pi, outer, aa);
    const auto base = static_cast<std::uint32_t>(out.vertices.size());
    out.vertices.push_back(Vertex(center, color));
    for (int i = 0; i <= steps; ++i) {
        const float angle = Pi * static_cast<float>(i) / static_cast<float>(steps);
        const Float2 offset = normal * std::cos(angle) + direction * std::sin(angle);
        out.vertices.push_back(Vertex(center + offset * core, color));
        out.vertices.push_back(Vertex(center + offset * outer, Clear));
    }
    for (int i = 0; i < steps; ++i) {
        const std::uint32_t a = base + 1 + 2 * static_cast<std::uint32_t>(i);
        const std::uint32_t b = a + 2;
        out.indices.insert(out.indices.end(), {base, a, b});
        Quad(out, a, a + 1, b + 1, b);
    }
}

void JoinRows(Ribbon& ribbon, Float2 point, Float2 incoming, Float2 outgoing, float inLength, float outLength,
              float halfWidth, const StrokeParams& params, float outer) {
    const Float2 n0 = Perpendicular(incoming);
    const Float2 n1 = Perpendicular(outgoing);
    const float cross = Cross(incoming, outgoing);
    const Float2 sum = n0 + n1;
    if (std::abs(cross) < 1e-5f && Dot(incoming, outgoing) > 0.0f) { // Straight on
        ribbon.Add({point, n0, -n0});
        return;
    }
    const bool reversal = Length(sum) < 1e-4f;
    const Float2 bisector = reversal ? n0 : Normalized(sum);
    const float miter = reversal ? 1.0f : 1.0f / std::max(Dot(bisector, n0), 1e-4f);
    // The inner corner never reaches past the shorter neighbouring segment.
    const float innerLimit = std::max(1.0f, std::min(inLength, outLength) / std::max(halfWidth, 1e-6f));
    const float inner = std::min(miter, innerLimit);
    const bool outerLeft = cross < 0.0f; // Turning towards -normal: the +normal side is outside
    const Float2 innerOffset = outerLeft ? -bisector * inner : bisector * inner;
    if (params.join == StrokeJoin::Miter && miter <= params.miterLimit && !reversal) {
        const Float2 outerOffset = outerLeft ? bisector * miter : -bisector * miter;
        ribbon.Add(outerLeft ? Row{point, outerOffset, innerOffset} : Row{point, innerOffset, outerOffset});
        return;
    }
    const Float2 from = outerLeft ? n0 : -n0;
    const Float2 to = outerLeft ? n1 : -n1;
    int steps = 1;
    if (params.join == StrokeJoin::Round) {
        const float angle = std::acos(std::clamp(Dot(from, to), -1.0f, 1.0f));
        steps = RoundSteps(angle, outer, params.aa);
    }
    const float total = std::atan2(Cross(from, to), Dot(from, to));
    for (int i = 0; i <= steps; ++i) {
        const Float2 side = Rotated(from, total * static_cast<float>(i) / static_cast<float>(steps));
        ribbon.Add(outerLeft ? Row{point, side, innerOffset} : Row{point, innerOffset, side});
    }
}

float SignedArea(std::span<const Float2> points) {
    float area = 0.0f;
    for (std::size_t i = 0; i < points.size(); ++i)
        area += Cross(points[i], points[(i + 1) % points.size()]);
    return area * 0.5f;
}

// Appends per-corner offsets (per unit of distance from each edge), clamped at sharp corners. They point out of the
// ring when 'side' is 1 and into it when -1 (a hole's edge, seen from the filled area around it).
void AppendCornerOffsets(std::span<const Float2> points, float limit, float side, std::vector<Float2>& offsets) {
    const std::size_t count = points.size();
    side *= SignedArea(points) > 0.0f ? 1.0f : -1.0f;
    const auto outward = [&](std::size_t i) {
        const Float2 edge = Normalized(points[(i + 1) % count] - points[i]);
        return Float2{edge.y, -edge.x} * side;
    };
    for (std::size_t i = 0; i < count; ++i) {
        const Float2 previous = outward((i + count - 1) % count);
        const Float2 next = outward(i);
        const Float2 bisector = Normalized(previous + next, next);
        const float scale = std::min(1.0f / std::max(Dot(bisector, next), 1e-4f), limit);
        offsets.push_back(bisector * scale);
    }
}

// Fills one outline and its holes ('rings', the outline first) with an anti-aliasing fringe along every ring.
void FillRings(MeshBuffer& out, std::span<const std::span<const Float2>> rings, PaintColor color, float aa) {
    std::vector<Float2>& offsets = out.offsets;
    offsets.clear();
    for (std::size_t r = 0; r < rings.size(); ++r)
        AppendCornerOffsets(rings[r], MaxFillMiter, r == 0 ? 1.0f : -1.0f, offsets);
    const float half = aa * 0.5f;
    const auto base = static_cast<std::uint32_t>(out.vertices.size());
    const auto total = static_cast<std::uint32_t>(offsets.size());
    std::size_t at = 0;
    for (const std::span<const Float2> ring : rings)
        for (const Float2 point : ring)
            out.vertices.push_back(Vertex(point - offsets[at++] * half, color));
    out.triangulator.Triangulate(rings, base, out.indices);
    at = 0;
    for (const std::span<const Float2> ring : rings)
        for (const Float2 point : ring)
            out.vertices.push_back(Vertex(point + offsets[at++] * half, Clear));
    std::uint32_t first = base;
    for (const std::span<const Float2> ring : rings) {
        const auto n = static_cast<std::uint32_t>(ring.size());
        for (std::uint32_t i = 0; i < n; ++i) {
            const std::uint32_t next = (i + 1) % n;
            Quad(out, first + i, first + total + i, first + total + next, first + next);
        }
        first += n;
    }
}

float DistanceToSegment(Float2 point, Float2 a, Float2 b) {
    const Float2 edge = b - a;
    const float span = LengthSquared(edge);
    const float t = span > 0.0f ? std::clamp(Dot(point - a, edge) / span, 0.0f, 1.0f) : 0.0f;
    return Length(point - (a + edge * t));
}

float DistanceToRings(Float2 point, const Regions& edge) {
    float nearest = std::numeric_limits<float>::max();
    for (std::size_t r = 0; r < edge.rings.size(); ++r) {
        const std::span<const Float2> ring = edge.Ring(r);
        for (std::size_t i = 0; i < ring.size(); ++i)
            nearest = std::min(nearest, DistanceToSegment(point, ring[i], ring[(i + 1) % ring.size()]));
    }
    return nearest;
}
} // namespace

void Deduplicate(std::span<const Float2> points, bool closed, std::vector<Float2>& out) {
    out.clear();
    for (const Float2 point : points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) continue;
        if (out.empty() || LengthSquared(point - out.back()) > SamePoint * SamePoint) out.push_back(point);
    }
    if (closed && out.size() > 1 && LengthSquared(out.front() - out.back()) <= SamePoint * SamePoint) out.pop_back();
}

void StrokePolyline(MeshBuffer& out, std::span<const Float2> input, bool closed, const StrokeParams& params,
                    PaintColor color) {
    std::vector<Float2>& points = out.points;
    Deduplicate(input, closed, points);
    const std::size_t count = points.size();
    if (count < 2 || !(params.width > 0.0f) || !(params.aa > 0.0f)) return;
    if (count < 3) closed = false;
    float width = params.width;
    if (width < params.aa) { // Hairline: one pixel wide, fainter unless hard
        if (!params.hard) color = Scaled(color, width / params.aa);
        width = params.aa;
    }
    const float fringe = params.hard ? params.aa * HardFringe : params.aa;
    const float halfWidth = width * 0.5f;
    const float core = std::max(halfWidth - fringe * 0.5f, 0.0f);
    const float outer = halfWidth + fringe * 0.5f;
    const std::size_t segments = closed ? count : count - 1;
    std::vector<Float2>& directions = out.directions;
    directions.resize(segments);
    std::vector<float>& lengths = out.lengths;
    lengths.resize(segments);
    for (std::size_t i = 0; i < segments; ++i) {
        const Float2 delta = points[(i + 1) % count] - points[i];
        lengths[i] = Length(delta);
        directions[i] = delta / lengths[i];
    }

    Ribbon ribbon(out, core, outer, color);
    const float endFringe = fringe * 0.5f;
    if (!closed) {
        const Float2 start = points[0];
        const Float2 direction = directions[0];
        const Float2 normal = Perpendicular(direction);
        if (params.cap == StrokeCap::Round) {
            RoundCap(out, start, -direction, core, outer, params.aa, color);
            ribbon.Add({start, normal, -normal});
        } else {
            const Float2 end = params.cap == StrokeCap::Square ? start - direction * halfWidth : start;
            ribbon.Add({end - direction * endFringe, normal, -normal, true});
            ribbon.Add({end + direction * endFringe, normal, -normal});
        }
    }
    const std::size_t firstJoint = closed ? 0 : 1;
    const std::size_t lastJoint = closed ? count : count - 1;
    for (std::size_t i = firstJoint; i < lastJoint; ++i) {
        const std::size_t previous = (i + segments - 1) % segments;
        JoinRows(ribbon, points[i], directions[previous], directions[i % segments], lengths[previous],
                 lengths[i % segments], halfWidth, params, outer);
    }
    if (closed) {
        ribbon.Close();
        return;
    }
    const Float2 end = points[count - 1];
    const Float2 direction = directions[segments - 1];
    const Float2 normal = Perpendicular(direction);
    if (params.cap == StrokeCap::Round) {
        ribbon.Add({end, normal, -normal});
        RoundCap(out, end, direction, core, outer, params.aa, color);
    } else {
        const Float2 tip = params.cap == StrokeCap::Square ? end + direction * halfWidth : end;
        ribbon.Add({tip - direction * endFringe, normal, -normal});
        ribbon.Add({tip + direction * endFringe, normal, -normal, true});
    }
}

void FillPolygon(MeshBuffer& out, std::span<const Float2> input, PaintColor color, float aa) {
    std::vector<Float2>& points = out.points;
    Deduplicate(input, true, points);
    if (points.size() < 3 || std::abs(SignedArea(points)) < SamePoint) return;
    const std::span<const Float2> outline = points;
    FillRings(out, std::span(&outline, 1), color, aa);
}

void FillRegions(MeshBuffer& out, const Regions& regions, PaintColor color, float aa) {
    std::uint32_t first = 0;
    for (const std::uint32_t end : regions.ends) {
        const std::uint32_t outline = first;
        first = end;
        if (regions.rings[outline].count < 3) continue; // Without its outline, its holes would be filled instead
        out.rings.clear();
        for (std::uint32_t r = outline; r < end; ++r)
            if (regions.rings[r].count >= 3) out.rings.push_back(regions.Ring(r));
        FillRings(out, out.rings, color, aa);
    }
}

void ShadeGradient(MeshBuffer& out, std::size_t first, PaintColor from, PaintColor to, const Gradient& gradient,
                   const Affine2& toLocal) {
    const Float2 axis = gradient.end - gradient.start;
    const float span = std::max(LengthSquared(axis), 1e-12f);
    for (std::size_t i = first; i < out.vertices.size(); ++i) {
        PaintVertex& vertex = out.vertices[i];
        const Float2 local = toLocal.Apply({vertex.x, vertex.y});
        vertex.u = local.x;
        vertex.v = local.y;
        // Coverage must interpolate separately from both colours, including transparent gradient endpoints.
        vertex.shape3 = static_cast<float>(vertex.color.a) / 255.0f;
        vertex.color = from;
        vertex.color2 = to;
        if (gradient.kind == GradientKind::Linear) {
            vertex.flags = PaintGradientLinear | PaintCoverage;
            vertex.style0 = Dot(local - gradient.start, axis) / span;
        } else {
            vertex.flags = PaintGradientRadial | PaintCoverage;
            vertex.style0 = gradient.radius > 0.0f ? 1.0f / gradient.radius : 0.0f;
        }
    }
}

void FadeRegions(MeshBuffer& out, const Regions& band, const Regions& edge, float width, PaintColor color) {
    if (!(width > 0.0f)) return;
    std::uint32_t first = 0;
    for (const std::uint32_t end : band.ends) {
        const std::uint32_t outline = first;
        first = end;
        if (band.rings[outline].count < 3) continue;
        const auto base = static_cast<std::uint32_t>(out.vertices.size());
        out.rings.clear();
        for (std::uint32_t r = outline; r < end; ++r) {
            const std::span<const Float2> ring = band.Ring(r);
            out.rings.push_back(ring);
            for (const Float2 point : ring) {
                const float strength = std::clamp(1.0f - DistanceToRings(point, edge) / width, 0.0f, 1.0f);
                out.vertices.push_back(Vertex(point, Scaled(color, strength)));
            }
        }
        out.triangulator.Triangulate(out.rings, base, out.indices);
    }
}

void FlattenPath(const Path& path, const Affine2& transform, float tolerance, std::vector<Float2>& points,
                 std::vector<Contour>& contours) {
    points.clear();
    contours.clear();
    const auto& verbs = PathAccess::Verbs(path);
    const auto& source = PathAccess::Points(path);
    const float safeTolerance = std::max(tolerance, 1e-3f);
    std::size_t at = 0;
    Float2 current;
    const auto open = [&](Float2 start) {
        contours.push_back({static_cast<std::uint32_t>(points.size()), 0, false});
        points.push_back(start);
        current = start;
    };
    const auto add = [&](Float2 point) {
        if (contours.empty()) open(current);
        points.push_back(point);
        current = point;
    };
    for (const PathAccess::Verb verb : verbs) {
        switch (verb) {
        case PathAccess::Verb::Move: open(transform.Apply(source[at++])); break;
        case PathAccess::Verb::Line: add(transform.Apply(source[at++])); break;
        case PathAccess::Verb::Quad: {
            const Float2 p0 = current;
            const Float2 p1 = transform.Apply(source[at]);
            const Float2 p2 = transform.Apply(source[at + 1]);
            at += 2;
            const float curvature = Length(p0 - p1 * 2.0f + p2);
            const int steps = std::clamp(static_cast<int>(std::ceil(std::sqrt(curvature / (4.0f * safeTolerance)))), 1,
                                         MaxCurveSteps);
            for (int i = 1; i <= steps; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                const float u = 1.0f - t;
                add(p0 * (u * u) + p1 * (2.0f * u * t) + p2 * (t * t));
            }
            break;
        }
        case PathAccess::Verb::Cubic: {
            const Float2 p0 = current;
            const Float2 p1 = transform.Apply(source[at]);
            const Float2 p2 = transform.Apply(source[at + 1]);
            const Float2 p3 = transform.Apply(source[at + 2]);
            at += 3;
            const float curvature = std::max(Length(p0 - p1 * 2.0f + p2), Length(p1 - p2 * 2.0f + p3));
            const int steps = std::clamp(
                static_cast<int>(std::ceil(std::sqrt(3.0f * curvature / (4.0f * safeTolerance)))), 1, MaxCurveSteps);
            for (int i = 1; i <= steps; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(steps);
                const float u = 1.0f - t;
                add(p0 * (u * u * u) + p1 * (3.0f * u * u * t) + p2 * (3.0f * u * t * t) + p3 * (t * t * t));
            }
            break;
        }
        case PathAccess::Verb::Close:
            if (!contours.empty()) {
                contours.back().closed = true;
                current = points[contours.back().first];
            }
            break;
        }
        if (!contours.empty())
            contours.back().count = static_cast<std::uint32_t>(points.size()) - contours.back().first;
    }
}
