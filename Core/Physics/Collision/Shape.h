#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include "Utilities/Geometry/Float2.h"

enum class ShapeKind { Circle, Box, Capsule, Segment, Polygon };

// A collision shape in its own coordinates, placed in the world by a Pose. A plain value: copy it freely and keep
// constants of it. All shapes are convex; split anything else into several. Invalid sizes or points throw
// std::invalid_argument.
class Shape {
public:
    static constexpr std::size_t MaxPoints = 8;

    [[nodiscard]] static Shape Circle(float radius, Float2 center = {});
    [[nodiscard]] static Shape Box(float halfWidth, float halfHeight, Float2 center = {});
    // A segment with thickness: every point within 'radius' of a..b.
    [[nodiscard]] static Shape Capsule(Float2 a, Float2 b, float radius);
    // A line without thickness, such as a wall or a laser.
    [[nodiscard]] static Shape Segment(Float2 a, Float2 b);
    // 3 to 8 corners in order, either direction; the outline must be convex.
    [[nodiscard]] static Shape Polygon(std::span<const Float2> points);

    [[nodiscard]] ShapeKind Kind() const { return _kind; }
    // Circle: the center. Capsule and Segment: the two ends. Box and Polygon: the corners.
    [[nodiscard]] std::span<const Float2> Points() const { return {_points.data(), _count}; }
    // Circle and Capsule only; 0 for the others.
    [[nodiscard]] float Radius() const { return _radius; }

private:
    Shape() = default;
    std::array<Float2, MaxPoints> _points{};
    std::size_t _count = 0;
    float _radius = 0.0f;
    ShapeKind _kind = ShapeKind::Circle;
};
