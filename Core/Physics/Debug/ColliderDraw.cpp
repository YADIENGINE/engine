#include "Physics/Debug/ColliderDraw.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <vector>
#include "Graphics/Paint/PaintStyle.h"
#include "Graphics/Paint/Painter.h"
#include "Physics/Collision/CollisionWorld.h"
#include "Utilities/MathUtils.h"

namespace {
constexpr std::array<Color4, 6> LayerColors{Color4{90, 220, 255, 255},  Color4{255, 120, 150, 255},
                                            Color4{130, 240, 140, 255}, Color4{255, 210, 90, 255},
                                            Color4{190, 150, 255, 255}, Color4{255, 160, 90, 255}};
constexpr int CapSegments = 8;

Color4 ColorOf(CollisionMask layers) {
    return LayerColors[static_cast<std::size_t>(std::countr_zero(layers.bits)) % LayerColors.size()];
}
Float2 ToWorld(const Pose& pose, Float2 local) { return Rotated(local, pose.angle) + pose.position; }
// A half circle around 'center', starting at 'start' radians and turning clockwise on screen.
void AddCap(std::vector<Float2>& outline, Float2 center, float start, float radius) {
    for (int i = 0; i <= CapSegments; ++i)
        outline.push_back(center + Float2::FromAngle(start + Pi * static_cast<float>(i) / CapSegments, radius));
}
void DrawCollider(Painter& paint, const ColliderView& collider, const ColliderDrawStyle& style) {
    const Color4 color = ColorOf(collider.layers);
    const LineStyle line{.width = style.width, .color = color};
    const auto points = collider.shape.Points();
    switch (collider.shape.Kind()) {
    case ShapeKind::Circle:
        paint.Circle(ToWorld(collider.pose, points[0]), collider.shape.Radius(), Outline(style.width, color));
        return;
    case ShapeKind::Segment:
        paint.Line(ToWorld(collider.pose, points[0]), ToWorld(collider.pose, points[1]), line);
        return;
    case ShapeKind::Capsule: {
        const Float2 a = ToWorld(collider.pose, points[0]), b = ToWorld(collider.pose, points[1]);
        const Float2 along = b - a;
        const float angle = std::atan2(along.y, along.x);
        std::vector<Float2> outline;
        AddCap(outline, b, angle - Pi / 2.0f, collider.shape.Radius());
        AddCap(outline, a, angle + Pi / 2.0f, collider.shape.Radius());
        paint.Polyline(outline, line, true);
        return;
    }
    case ShapeKind::Box:
    case ShapeKind::Polygon: {
        std::vector<Float2> outline;
        for (const Float2 point : points)
            outline.push_back(ToWorld(collider.pose, point));
        paint.Polyline(outline, line, true);
        return;
    }
    }
}
} // namespace

void DrawColliders(Painter& paint, const CollisionWorld& world, const ColliderDrawStyle& style) {
    world.ForEach([&](const ColliderView& collider) {
        if (collider.layers.Overlaps(style.layers)) DrawCollider(paint, collider, style);
    });
}
