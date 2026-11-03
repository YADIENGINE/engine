#include "Physics/Collision/Collision.h"
#include <box2d/collision.h>
#include <box2d/math_functions.h>
#include "Physics/Internals/Box2dBridge.h"

namespace {
// Box2D's contact functions come in a fixed order: the polygon first, then the capsule, then the circle.
int ContactRank(const Shape& shape) {
    switch (shape.Kind()) {
    case ShapeKind::Circle : return 0;
    case ShapeKind::Capsule:
    case ShapeKind::Segment: return 1;
    case ShapeKind::Box    :
    case ShapeKind::Polygon: return 2;
    }
    return 0;
}
b2Circle CircleOf(const Shape& shape) { return {ToBox2d(shape.Points()[0]), shape.Radius()}; }
// A segment is a capsule without thickness, which is also how Box2D handles it.
b2Capsule CapsuleOf(const Shape& shape) {
    return {ToBox2d(shape.Points()[0]), ToBox2d(shape.Points()[1]), shape.Radius()};
}
b2Polygon PolygonOf(const Shape& shape) {
    const auto points = shape.Points();
    if (shape.Kind() == ShapeKind::Box) {
        const Float2 center = (points[0] + points[2]) * 0.5f, half = (points[2] - points[0]) * 0.5f;
        return b2MakeOffsetBox(half.x, half.y, ToBox2d(center), b2Rot_identity);
    }
    const b2ShapeProxy proxy = ProxyOf(shape);
    const b2Hull hull = b2ComputeHull(proxy.points, proxy.count);
    return b2MakePolygon(&hull, 0.0f);
}
b2LocalManifold ManifoldOf(const Shape& a, const Shape& b, b2Transform relative) {
    if (ContactRank(a) == 2) {
        const b2Polygon polygon = PolygonOf(a);
        if (ContactRank(b) == 2) {
            const b2Polygon other = PolygonOf(b);
            return b2CollidePolygons(&polygon, &other, relative);
        }
        if (ContactRank(b) == 1) {
            const b2Capsule other = CapsuleOf(b);
            return b2CollidePolygonAndCapsule(&polygon, &other, relative);
        }
        const b2Circle other = CircleOf(b);
        return b2CollidePolygonAndCircle(&polygon, &other, relative);
    }
    if (ContactRank(a) == 1) {
        const b2Capsule capsule = CapsuleOf(a);
        if (ContactRank(b) == 1) {
            const b2Capsule other = CapsuleOf(b);
            return b2CollideCapsules(&capsule, &other, relative);
        }
        const b2Circle other = CircleOf(b);
        return b2CollideCapsuleAndCircle(&capsule, &other, relative);
    }
    const b2Circle circle = CircleOf(a), other = CircleOf(b);
    return b2CollideCircles(&circle, &other, relative);
}
b2DistanceOutput DistanceBetween(const b2ShapeProxy& a, const Pose& poseA, const b2ShapeProxy& b, const Pose& poseB) {
    b2DistanceInput input{};
    input.proxyA = a;
    input.proxyB = b;
    input.transform = b2InvMulTransforms(ToBox2d(poseA), ToBox2d(poseB));
    input.useRadii = true;
    b2SimplexCache cache{};
    return b2ComputeShapeDistance(&input, &cache, nullptr, 0);
}
std::optional<CastHit> Cast(const b2ShapeProxy& moving, const Pose& from, Float2 move, const Shape& target,
                            const Pose& pose) {
    const b2ShapeProxy targetProxy = ProxyOf(target);
    if (move == Float2{}) { // Nothing to sweep: all that can be true is that it already overlaps
        if (DistanceBetween(targetProxy, pose, moving, from).distance > 0.0f) return std::nullopt;
        return CastHit{from.position, {}, 0.0f};
    }
    const b2Transform targetTransform = ToBox2d(pose);
    b2ShapeCastPairInput input{};
    input.proxyA = targetProxy;
    input.proxyB = moving;
    input.transform = b2InvMulTransforms(targetTransform, ToBox2d(from));
    input.translationB = b2InvRotateVector(targetTransform.q, ToBox2d(move));
    input.maxFraction = 1.0f;
    const b2CastOutput output = b2CastShape(&input);
    if (!output.hit) return std::nullopt;
    return CastHit{FromBox2d(b2TransformPoint(targetTransform, output.point)),
                   FromBox2d(b2RotateVector(targetTransform.q, output.normal)), output.fraction};
}
b2ShapeProxy PointProxy(Float2 point) {
    const b2Vec2 at = ToBox2d(point);
    return b2MakeProxy(&at, 1, 0.0f);
}
} // namespace

bool ShapesOverlap(const Shape& a, const Pose& poseA, const Shape& b, const Pose& poseB) {
    return ShapeDistance(a, poseA, b, poseB) <= 0.0f;
}
bool ShapeContains(const Shape& shape, const Pose& pose, Float2 point) {
    return DistanceBetween(ProxyOf(shape), pose, PointProxy(point), Pose{}).distance <= 0.0f;
}
float ShapeDistance(const Shape& a, const Pose& poseA, const Shape& b, const Pose& poseB) {
    return DistanceBetween(ProxyOf(a), poseA, ProxyOf(b), poseB).distance;
}
std::optional<CollisionContact> ShapeContact(const Shape& a, const Pose& poseA, const Shape& b, const Pose& poseB) {
    const bool swapped = ContactRank(a) < ContactRank(b);
    const Shape& first = swapped ? b : a;
    const Shape& second = swapped ? a : b;
    const b2Transform frame = ToBox2d(swapped ? poseB : poseA);
    const b2Transform other = ToBox2d(swapped ? poseA : poseB);
    const b2LocalManifold manifold = ManifoldOf(first, second, b2InvMulTransforms(frame, other));
    if (manifold.pointCount == 0) return std::nullopt;
    const b2LocalManifoldPoint* deepest = &manifold.points[0];
    if (manifold.pointCount == 2 && manifold.points[1].separation < deepest->separation) deepest = &manifold.points[1];
    if (deepest->separation > 0.0f) return std::nullopt; // Box2D also reports shapes that are about to touch
    Float2 normal = FromBox2d(b2RotateVector(frame.q, manifold.normal));
    if (normal == Float2{}) normal = {1.0f, 0.0f}; // Same center: any direction separates them
    if (swapped) normal = -normal;
    return CollisionContact{normal, -deepest->separation, FromBox2d(b2TransformPoint(frame, deepest->point))};
}
std::optional<CastHit> CastRay(Float2 from, Float2 to, const Shape& target, const Pose& pose) {
    return Cast(PointProxy(from), Pose{}, to - from, target, pose);
}
std::optional<CastHit> CastShape(const Shape& moving, const Pose& from, Float2 move, const Shape& target,
                                 const Pose& pose) {
    b2ShapeProxy proxy = ProxyOf(moving);
    const b2Transform transform = ToBox2d(from);
    // The moving proxy is passed with its pose folded in, so Cast needs only the translation.
    for (int i = 0; i < proxy.count; ++i)
        proxy.points[i] = b2TransformPoint(transform, proxy.points[i]);
    return Cast(proxy, Pose{}, move, target, pose);
}
