#pragma once
#include <span>
#include "Camera/CameraSettings.h"
#include "Camera/CameraTarget.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/FloatRect.h"

// Camera2D's geometry, one pure function per rule, so each is tested on its own. Engine-private.

// The targets with a weight above 0: their weighted middle, and the box around their circles. 'any' is false when
// none counts.
struct CameraGroup {
    Float2 center{};
    FloatRect bounds{};
    bool any = false;
};
[[nodiscard]] CameraGroup GatherTargets(std::span<const CameraTarget> targets);

// World size of what a viewport shows at a zoom.
[[nodiscard]] Float2 VisibleSize(const FloatRect& viewport, float zoom);

// 'goal' moved the least so that 'focus' lies within 'halfZone' of it on each axis.
[[nodiscard]] Float2 ApplyDeadZone(Float2 goal, Float2 focus, Float2 halfZone);

// The zoom that shows 'bounds' with 'zoom.padding' canvas units around it in 'viewport', within [zoom.min, zoom.max].
// A box with no size gets zoom.max.
[[nodiscard]] float FitZoom(const FloatRect& bounds, const FloatRect& viewport, const CameraZoom& zoom);

// 'center' moved the least so that 'bounds' stays inside the visible size around it, 'margin' world units inside its
// edges. On an axis where it cannot fit, the bounds are centred.
[[nodiscard]] Float2 KeepInView(Float2 center, const FloatRect& bounds, Float2 visibleSize, float margin);

// 'center' moved the least so that the visible size around it stays inside 'limits'; centred where it cannot.
[[nodiscard]] Float2 Confine(Float2 center, Float2 visibleSize, const FloatRect& limits);

// The view's target moved by less than a canvas unit so that the world moves by whole canvas units only.
[[nodiscard]] Float2 SnapToPixels(Float2 target, Float2 offset, float zoom);
