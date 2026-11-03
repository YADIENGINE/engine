#include "Physics/Internals/Box2dBridge.h"
#include <array>

b2ShapeProxy ProxyOf(const Shape& shape) {
    std::array<b2Vec2, Shape::MaxPoints> points{};
    const auto source = shape.Points();
    for (std::size_t i = 0; i < source.size(); ++i)
        points[i] = ToBox2d(source[i]);
    return b2MakeProxy(points.data(), static_cast<int>(source.size()), shape.Radius());
}
b2AABB BoundsOf(const Shape& shape, const Pose& pose) {
    const b2Transform transform = ToBox2d(pose);
    const auto source = shape.Points();
    b2Vec2 lower = b2TransformPoint(transform, ToBox2d(source[0])), upper = lower;
    for (std::size_t i = 1; i < source.size(); ++i) {
        const b2Vec2 point = b2TransformPoint(transform, ToBox2d(source[i]));
        lower = b2Min(lower, point);
        upper = b2Max(upper, point);
    }
    const b2Vec2 radius = {shape.Radius(), shape.Radius()};
    return {b2Sub(lower, radius), b2Add(upper, radius)};
}
