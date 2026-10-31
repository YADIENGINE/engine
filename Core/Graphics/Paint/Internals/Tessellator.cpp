#include "Graphics/Paint/Internals/Tessellator.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Paint/Internals/PathAccess.h"
#include "Utilities/MathUtils.h"

namespace {
constexpr float SamePoint = 1e-4f;
constexpr float RoundTolerance = 0.25f; // Of the fringe width: how far round joins / caps may deviate from a circle
constexpr int MaxRoundSteps = 32;
constexpr float MaxFillMiter = 4.0f; // Polygon corner offsets beyond this many edge widths are clamped
constexpr float MaxHaloMiter = 2.0f;
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

// Per-corner outward offset (per unit of distance from each edge), clamped at sharp corners.
void CornerOffsets(std::span<const Float2> points, float limit, std::vector<Float2>& offsets) {
    const std::size_t count = points.size();
    const float side = SignedArea(points) > 0.0f ? 1.0f : -1.0f;
    const auto outward = [&](std::size_t i) {
        const Float2 edge = Normalized(points[(i + 1) % count] - points[i]);
        return Float2{edge.y, -edge.x} * side;
    };
    offsets.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        const Float2 previous = outward((i + count - 1) % count);
        const Float2 next = outward(i);
        const Float2 bisector = Normalized(previous + next, next);
        const float scale = std::min(1.0f / std::max(Dot(bisector, next), 1e-4f), limit);
        offsets[i] = bisector * scale;
    }
}

bool InsideTriangle(Float2 p, Float2 a, Float2 b, Float2 c) {
    const float d1 = Cross(b - a, p - a);
    const float d2 = Cross(c - b, p - b);
    const float d3 = Cross(a - c, p - c);
    const bool negative = d1 < 0.0f || d2 < 0.0f || d3 < 0.0f;
    const bool positive = d1 > 0.0f || d2 > 0.0f || d3 > 0.0f;
    return !(negative && positive);
}

// Triangulates the polygon whose vertices are out.vertices[base .. base + count) (ear clipping, O(n^2)).
void Triangulate(MeshBuffer& out, std::span<const Float2> points, std::uint32_t base) {
    const std::size_t count = points.size();
    const float side = SignedArea(points) > 0.0f ? 1.0f : -1.0f;
    bool convex = true;
    for (std::size_t i = 0; i < count && convex; ++i) {
        const Float2 a = points[i];
        const Float2 b = points[(i + 1) % count];
        const Float2 c = points[(i + 2) % count];
        convex = Cross(b - a, c - b) * side >= 0.0f;
    }
    std::vector<std::uint32_t>& remaining = out.remaining;
    remaining.resize(count);
    for (std::size_t i = 0; i < count; ++i)
        remaining[i] = static_cast<std::uint32_t>(i);
    if (!convex) {
        std::size_t guard = 0;
        while (remaining.size() > 3 && guard < remaining.size()) {
            bool clipped = false;
            for (std::size_t i = 0; i < remaining.size(); ++i) {
                const std::size_t n = remaining.size();
                const std::uint32_t ia = remaining[(i + n - 1) % n];
                const std::uint32_t ib = remaining[i];
                const std::uint32_t ic = remaining[(i + 1) % n];
                const Float2 a = points[ia];
                const Float2 b = points[ib];
                const Float2 c = points[ic];
                if (Cross(b - a, c - b) * side <= 0.0f) continue; // Reflex corner
                bool ear = true;
                for (const std::uint32_t other : remaining) {
                    if (other == ia || other == ib || other == ic) continue;
                    if (InsideTriangle(points[other], a, b, c)) {
                        ear = false;
                        break;
                    }
                }
                if (!ear) continue;
                out.indices.insert(out.indices.end(), {base + ia, base + ib, base + ic});
                remaining.erase(remaining.begin() + static_cast<std::ptrdiff_t>(i));
                clipped = true;
                break;
            }
            guard = clipped ? 0 : guard + 1;
            if (!clipped) break; // Self-intersecting: fan the rest
        }
    }
    for (std::size_t i = 1; i + 1 < remaining.size(); ++i)
        out.indices.insert(out.indices.end(), {base + remaining[0], base + remaining[i], base + remaining[i + 1]});
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
    if (width < params.aa) { // Hairline: one fringe wide, fainter
        color = Scaled(color, width / params.aa);
        width = params.aa;
    }
    const float halfWidth = width * 0.5f;
    const float core = std::max(halfWidth - params.aa * 0.5f, 0.0f);
    const float outer = halfWidth + params.aa * 0.5f;
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
    const float fringe = params.aa * 0.5f;
    if (!closed) {
        const Float2 start = points[0];
        const Float2 direction = directions[0];
        const Float2 normal = Perpendicular(direction);
        if (params.cap == StrokeCap::Round) {
            RoundCap(out, start, -direction, core, outer, params.aa, color);
            ribbon.Add({start, normal, -normal});
        } else {
            const Float2 end = params.cap == StrokeCap::Square ? start - direction * halfWidth : start;
            ribbon.Add({end - direction * fringe, normal, -normal, true});
            ribbon.Add({end + direction * fringe, normal, -normal});
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
        ribbon.Add({tip - direction * fringe, normal, -normal});
        ribbon.Add({tip + direction * fringe, normal, -normal, true});
    }
}

void FillPolygon(MeshBuffer& out, std::span<const Float2> input, PaintColor color, float aa) {
    std::vector<Float2>& points = out.points;
    Deduplicate(input, true, points);
    const std::size_t count = points.size();
    if (count < 3 || std::abs(SignedArea(points)) < SamePoint) return;
    std::vector<Float2>& offsets = out.offsets;
    CornerOffsets(points, MaxFillMiter, offsets);
    std::vector<Float2>& inner = out.directions;
    inner.resize(count);
    const float half = aa * 0.5f;
    for (std::size_t i = 0; i < count; ++i)
        inner[i] = points[i] - offsets[i] * half;
    const auto base = static_cast<std::uint32_t>(out.vertices.size());
    for (const Float2 point : inner)
        out.vertices.push_back(Vertex(point, color));
    for (std::size_t i = 0; i < count; ++i)
        out.vertices.push_back(Vertex(points[i] + offsets[i] * half, Clear));
    Triangulate(out, inner, base);
    const auto n = static_cast<std::uint32_t>(count);
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint32_t next = (i + 1) % n;
        Quad(out, base + i, base + n + i, base + n + next, base + next);
    }
}

void FillPolygon(MeshBuffer& out, std::span<const Float2> points, PaintColor from, PaintColor to,
                 const Gradient& gradient, const Affine2& toLocal, float aa) {
    const std::size_t first = out.vertices.size();
    FillPolygon(out, points, PaintColor{255, 255, 255, 255}, aa);
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

void PolygonHalo(MeshBuffer& out, std::span<const Float2> input, float width, PaintColor color) {
    std::vector<Float2>& points = out.points;
    Deduplicate(input, true, points);
    const std::size_t count = points.size();
    if (count < 3 || !(width > 0.0f)) return;
    std::vector<Float2>& offsets = out.offsets;
    CornerOffsets(points, MaxHaloMiter, offsets);
    const auto base = static_cast<std::uint32_t>(out.vertices.size());
    for (const Float2 point : points)
        out.vertices.push_back(Vertex(point, color));
    for (std::size_t i = 0; i < count; ++i)
        out.vertices.push_back(Vertex(points[i] + offsets[i] * width, Clear));
    const auto n = static_cast<std::uint32_t>(count);
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::uint32_t next = (i + 1) % n;
        Quad(out, base + i, base + n + i, base + n + next, base + next);
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
