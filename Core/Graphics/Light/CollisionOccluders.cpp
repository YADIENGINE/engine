#include "Graphics/Light/CollisionOccluders.h"
#include <array>
#include <cmath>
#include <vector>
#include "Graphics/Light/LightOccluders.h"
#include "Physics/Collision/CollisionWorld.h"
#include "Physics/Collision/Shape.h"
#include "Utilities/Math/Scalar.h"

namespace {
constexpr int CirclePoints = 16;
constexpr int CapPoints = 8; // Points in each half circle of a capsule

void AddArc(std::vector<Float2>& points, Float2 center, float radius, float start, float sweep, int count) {
    for (int i = 0; i < count; ++i) {
        const float angle = start + sweep * static_cast<float>(i) / static_cast<float>(count);
        points.push_back(center + Float2::FromAngle(angle, radius));
    }
}
} // namespace

void AddOccluders(LightOccluders& into, const CollisionWorld& world, CollisionMask mask, bool selfLit) {
    std::vector<Float2> points;
    world.ForEach([&](const ColliderView& collider) {
        if (!collider.layers.Overlaps(mask)) return;
        const Shape& shape = collider.shape;
        const Affine2 at = Affine2::Translate(collider.pose.position) * Affine2::Rotate(collider.pose.angle);
        const std::span<const Float2> own = shape.Points();
        switch (shape.Kind()) {
        case ShapeKind::Box:
        case ShapeKind::Polygon: into.Outline(own, at, selfLit); break;
        case ShapeKind::Segment: into.Line(own, at); break;
        case ShapeKind::Circle:
            points.clear();
            AddArc(points, own[0], shape.Radius(), 0.0f, 2.0f * Pi, CirclePoints);
            into.Outline(points, at, selfLit);
            break;
        case ShapeKind::Capsule: {
            const Float2 along = own[1] - own[0];
            const float side = std::atan2(along.y, along.x) + Pi * 0.5f;
            points.clear();
            AddArc(points, own[1], shape.Radius(), side - Pi, Pi, CapPoints);
            AddArc(points, own[0], shape.Radius(), side, Pi, CapPoints);
            into.Outline(points, at, selfLit);
            break;
        }
        }
    });
}
