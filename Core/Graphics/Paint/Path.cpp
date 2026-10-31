#include "Graphics/Paint/Path.h"
#include <algorithm>
#include <cmath>
#include "Utilities/MathUtils.h"

namespace {
constexpr float MaxArcStep = Pi / 2.0f; // One cubic per quarter circle

// Control point distance of a cubic approximating a circular arc of 'sweep' radians (unit radius).
float ArcHandle(float sweep) { return 4.0f / 3.0f * std::tan(sweep / 4.0f); }
} // namespace

void Path::EnsureContour() {
    if (_open) return;
    MoveTo(_points.empty() ? Float2{} : _points.back());
}

Path& Path::MoveTo(Float2 point) {
    _verbs.push_back(Verb::Move);
    _points.push_back(point);
    _open = true;
    return *this;
}

Path& Path::LineTo(Float2 point) {
    EnsureContour();
    _verbs.push_back(Verb::Line);
    _points.push_back(point);
    return *this;
}

Path& Path::QuadTo(Float2 control, Float2 to) {
    EnsureContour();
    _verbs.push_back(Verb::Quad);
    _points.push_back(control);
    _points.push_back(to);
    return *this;
}

Path& Path::CubicTo(Float2 control1, Float2 control2, Float2 to) {
    EnsureContour();
    _verbs.push_back(Verb::Cubic);
    _points.push_back(control1);
    _points.push_back(control2);
    _points.push_back(to);
    return *this;
}

Path& Path::ArcTo(Float2 center, float radius, float startAngle, float sweep) {
    const Float2 start = center + Float2::FromAngle(startAngle, radius);
    if (_open)
        LineTo(start);
    else
        MoveTo(start);
    if (sweep == 0.0f || radius <= 0.0f) return *this;
    const int steps = std::max(1, static_cast<int>(std::ceil(std::abs(sweep) / MaxArcStep)));
    const float step = sweep / static_cast<float>(steps);
    const float handle = ArcHandle(step) * radius;
    float angle = startAngle;
    for (int i = 0; i < steps; ++i) {
        const float next = angle + step;
        const Float2 from = center + Float2::FromAngle(angle, radius);
        const Float2 to = center + Float2::FromAngle(next, radius);
        CubicTo(from + Perpendicular(Float2::FromAngle(angle)) * handle,
                to - Perpendicular(Float2::FromAngle(next)) * handle, to);
        angle = next;
    }
    return *this;
}

Path& Path::Close() {
    if (!_open) return *this;
    _verbs.push_back(Verb::Close);
    _open = false;
    return *this;
}

Path& Path::AddPolygon(std::span<const Float2> points) {
    if (points.empty()) return *this;
    MoveTo(points.front());
    for (const Float2 point : points.subspan(1))
        LineTo(point);
    return Close();
}

Path& Path::AddCircle(Float2 center, float radius) {
    _open = false;
    ArcTo(center, radius, 0.0f, 2.0f * Pi);
    return Close();
}

Path& Path::AddRegularPolygon(Float2 center, int sides, float radius, float radians) {
    if (sides < 3) return *this;
    for (int i = 0; i < sides; ++i) {
        const Float2 point =
            center + Float2::FromAngle(radians + static_cast<float>(i) * 2.0f * Pi / static_cast<float>(sides), radius);
        if (i == 0)
            MoveTo(point);
        else
            LineTo(point);
    }
    return Close();
}

void Path::Clear() {
    _verbs.clear();
    _points.clear();
    _open = false;
}
