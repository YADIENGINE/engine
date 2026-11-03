#include "Physics/Collision/Shape.h"
#include <box2d/collision.h>
#include <cmath>
#include <stdexcept>
#include <string>
#include "Physics/Internals/Box2dBridge.h"

namespace {
void RequirePoint(Float2 point) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) throw std::invalid_argument("Shape: a point is not finite");
}
void RequirePositive(float value, const char* what) {
    if (!std::isfinite(value) || value <= 0.0f) throw std::invalid_argument(std::string("Shape: ") + what);
}
void RequireApart(Float2 a, Float2 b) {
    if (a == b) throw std::invalid_argument("Shape: the two ends are the same point");
}
} // namespace

Shape Shape::Circle(float radius, Float2 center) {
    RequirePositive(radius, "a circle needs a positive radius");
    RequirePoint(center);
    Shape shape;
    shape._kind = ShapeKind::Circle;
    shape._points[0] = center;
    shape._count = 1;
    shape._radius = radius;
    return shape;
}
Shape Shape::Box(float halfWidth, float halfHeight, Float2 center) {
    RequirePositive(halfWidth, "a box needs a positive half width");
    RequirePositive(halfHeight, "a box needs a positive half height");
    RequirePoint(center);
    Shape shape;
    shape._kind = ShapeKind::Box;
    shape._points[0] = center + Float2{-halfWidth, -halfHeight};
    shape._points[1] = center + Float2{halfWidth, -halfHeight};
    shape._points[2] = center + Float2{halfWidth, halfHeight};
    shape._points[3] = center + Float2{-halfWidth, halfHeight};
    shape._count = 4;
    return shape;
}
Shape Shape::Capsule(Float2 a, Float2 b, float radius) {
    RequirePositive(radius, "a capsule needs a positive radius");
    RequirePoint(a);
    RequirePoint(b);
    RequireApart(a, b);
    Shape shape;
    shape._kind = ShapeKind::Capsule;
    shape._points[0] = a;
    shape._points[1] = b;
    shape._count = 2;
    shape._radius = radius;
    return shape;
}
Shape Shape::Segment(Float2 a, Float2 b) {
    RequirePoint(a);
    RequirePoint(b);
    RequireApart(a, b);
    Shape shape;
    shape._kind = ShapeKind::Segment;
    shape._points[0] = a;
    shape._points[1] = b;
    shape._count = 2;
    return shape;
}
Shape Shape::Polygon(std::span<const Float2> points) {
    if (points.size() < 3 || points.size() > MaxPoints)
        throw std::invalid_argument(
            "Shape: a polygon needs 3 to 8 corners; split a larger outline into several shapes");
    Shape shape;
    shape._kind = ShapeKind::Polygon;
    shape._count = points.size();
    float turn = 0.0f;
    for (std::size_t i = 0; i < points.size(); ++i) {
        RequirePoint(points[i]);
        shape._points[i] = points[i];
        const Float2 next = points[(i + 1) % points.size()], after = points[(i + 2) % points.size()];
        const float cross = Cross(next - points[i], after - next);
        if (cross == 0.0f || (turn != 0.0f && (cross > 0.0f) != (turn > 0.0f)))
            throw std::invalid_argument("Shape: a polygon must be convex, without repeated or collinear corners");
        turn = cross;
    }
    const b2ShapeProxy proxy = ProxyOf(shape);
    if (b2ComputeHull(proxy.points, proxy.count).count < 3)
        throw std::invalid_argument("Shape: this polygon is too small or too thin to collide reliably");
    return shape;
}
