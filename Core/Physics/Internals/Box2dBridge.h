#pragma once
#include <box2d/collision.h>
#include <box2d/math_functions.h>
#include "Physics/Collision/Pose.h"
#include "Physics/Collision/Shape.h"

// The only place engine types meet Box2D's; no public header includes this one.
[[nodiscard]] inline b2Vec2 ToBox2d(Float2 value) { return {value.x, value.y}; }
[[nodiscard]] inline Float2 FromBox2d(b2Vec2 value) { return {value.x, value.y}; }
// Box2D's own sine and cosine, written for determinism; the standard library's may differ between builds.
[[nodiscard]] inline b2Transform ToBox2d(const Pose& pose) { return {ToBox2d(pose.position), b2MakeRot(pose.angle)}; }
[[nodiscard]] b2ShapeProxy ProxyOf(const Shape& shape);
// The world box around a posed shape.
[[nodiscard]] b2AABB BoundsOf(const Shape& shape, const Pose& pose);
