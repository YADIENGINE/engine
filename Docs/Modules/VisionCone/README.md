# VisionCone

CPU-only, opt-in 2D sight queries and sampled contours. Combine it with AI perception, a security camera, a flashlight,
or a view mask. The module owns no entities, time, renderer resources, detection memory or game rules.

## Setup and dependencies

Include only `Modules/VisionCone/VisionCone.h`. Link `EngineVisionCone` explicitly, for example:

```cmake
engine_add_simulation(MySimulation SOURCES Game/Simulation/MySimulation.cpp LIBRARIES EngineVisionCone)
```

`EngineVisionCone` is an `EXCLUDE_FROM_ALL` static library. Its sole direct target dependency is public `EnginePhysics`,
because its public collision adapter names `CollisionWorld`, `CollisionMask` and `ColliderId`. There is no graphics,
audio, runtime or editor dependency, and no automatic runtime work. Existing Physics transitive dependencies remain
unchanged. The library uses the engine's precise-float simulation flags; cross-platform bitwise identity is not promised.

## Public API

All operations belong to `VisionCone`. Supporting types in the same header are plain value contracts:

| Type | Meaning |
| --- | --- |
| `VisionConeSpec` | `origin`, clockwise-radian `direction`, `range` (default 100), `halfAngle` (default Pi / 4) |
| `VisionTrace` | Synchronous callback `(from, to) -> std::optional<float>`; first blocking fraction in the closed segment, or no hit |
| `VisionStatus` | `Visible`, `OutOfRange`, `OutsideAngle`, `Occluded` |
| `VisionPointResult` | `status`, world-unit `distance` (double), optional `blockingFraction` |
| `VisionSampleResult` | `total`, `inCone`, `visible` sample counts |
| `VisionOutlineOptions` | `segments` across the cone's arc, default 64 |

| Operation | Result |
| --- | --- |
| `Contains(spec, point)` | Geometric sector membership; no obstacles |
| `Query(spec, point, trace = {})` | Range, then angle, then occlusion; never traces a geometrically rejected point |
| `QuerySamples(spec, points, trace = {})` | Evaluates explicit target points in input order; returns counts |
| `CollisionTrace(world, blockers, ignore = {})` | Creates a callback borrowing the live collision world |
| `Outline(spec, options = {}, trace = {})` | Borrowed span of world-space contour vertices for drawing |

All but `Outline` are static; only contour generation needs an instance for its reusable buffer.

### Geometry and failure behavior

- Positive Y points down; positive angles turn clockwise from positive X. Any finite direction is accepted and wrapped.
- Range and angle boundaries are included. Angle comparisons allow less than `5e-7` radians for float roundoff.
  `halfAngle == Pi` is a full circle; sectors wider than 180 degrees are supported.
- The origin passes the angle test. Its query still invokes a supplied trace on the zero-length segment.
- `range == 0` disables sight, including at the origin; queries return `OutOfRange`, contours are empty, and no trace runs.
- Empty trace means no obstacles. A trace must accept zero-length segments and return the first blocker. Fractions
  zero (at the observer) and one (at the target) both occlude. Only `Occluded` has a `blockingFraction`.
- Negative range, nonfinite spec fields or query points, `halfAngle` outside `(0, Pi]`, and nonfinite/out-of-range trace
  fractions throw `std::invalid_argument`. Outline requires at least one segment for a sector, three for a full circle,
  and endpoints representable as finite `Float2` values. Specs/options are validated even when sight is disabled;
  empty sample queries also validate the spec.
- Callback exceptions propagate. Queries are not transactional: earlier callbacks in a sample/outline operation may
  already have run when a later input or callback fails. Normal standard-library allocation failures also propagate.

### Ownership and concurrency

Queries borrow their spans/callbacks only for the duration of the call and retain nothing. Concurrent queries are safe
when the callbacks and their data sources permit it. The collision callback copies the mask and ignored id, borrows the
world by reference, and observes subsequent world changes. Do not use it after the world dies or moves, or concurrently
with world mutation/queries unless that world supports it.

The contour span belongs to the instance and expires on its next `Outline` call (including a failing call), assignment,
move or destruction. Copy vertices into a caller-owned vector to keep a snapshot. Do not call `Outline` concurrently or
reenter it on the same instance. Its callback may use the stateless queries. There is no persistent perception state to
serialize; the consuming simulation owns its cone spec and AI state.

## AI and explicit target samples

The game finds candidate targets and supplies meaningful sample points. The module does not know what an enemy, body,
team or detection threshold is:

```cpp
#include "Modules/VisionCone/VisionCone.h"

VisionSampleResult Observe(const CollisionWorld& world, CollisionMask blockers, ColliderId observer,
                           const VisionConeSpec& sight, std::span<const Float2> targetSamples) {
    const auto trace = VisionCone::CollisionTrace(world, blockers, observer);
    return VisionCone::QuerySamples(sight, targetSamples, trace);
}
```

`inCone` includes occluded samples. `total` includes samples outside the cone. Empty input gives three zeros. A consumer
can require `visible > 0`, require all samples, or compute `double(visible) / total` after checking for zero. Awareness,
memory and enter/exit hysteresis remain in its AI system.

Exclude the target's colliders from the blocker mask, or they will correctly count as blockers, even at the endpoint.
Pass the observer's collider as `ignore` when its layer is included. For a tile grid or a separate sight material model,
supply a custom `VisionTrace` without changing the module. A zero mask is clear sight, not all layers.

## Contours, indicators and masks

A sector contour starts with the origin, then samples both arc ends and the points between in clockwise order. A full
circle contains only perimeter samples, without a repeated closing vertex. Consecutive duplicates and a repeated closing
vertex are removed; fewer than three vertices gives an empty contour. A blocker shortens the corresponding ray.

```cpp
#include "Modules/VisionCone/VisionCone.h"
#include "Graphics/Paint/Painter.h"

void DrawVisionIndicator(VisionCone& contours, Painter& paint, const VisionConeSpec& sight,
                         const VisionTrace& trace) {
    const auto points = contours.Outline(sight, {.segments = 64}, trace);
    if (points.empty()) return;
    paint.Polygon(points, Fill({80, 180, 255, 40}));
}
```

Call under the same camera as the world. Drawing these vertices into an existing `Surface` via `Painter::Target` also
allows composition as a mask; see [Painter scopes](../../Graphics/Painter.md#scopes). Contours use uniform angular
samples: narrow walls between rays can be missed, chords approximate the range arc, and very wide sectors need adequate
subdivision. They are visual approximations, never the authority for AI. Increase segments only when the view needs it.
Do not pass an empty contour to `Painter::Light` as an occlusion mask: that API interprets empty as unrestricted.

## Directional light composition

Map the same sector data into an existing light, leaving color, softness, intensity and falloff to rendering:

```cpp
#include "Modules/VisionCone/VisionCone.h"
#include "Graphics/Light/LightLayer.h"

void DrawVisionLight(LightLayer& layer, Painter& paint, const CameraView& view, const VisionConeSpec& sight,
                     std::span<const LightOccluders* const> occluders) {
    if (sight.range == 0.0f) return;
    const LightSource light{.at = sight.origin,
                            .radius = sight.range,
                            .color = {255, 230, 190, 255},
                            .cone = {.direction = sight.direction, .halfAngle = sight.halfAngle}};
    layer.Draw(paint, {.ambient = {20, 20, 25, 255}, .lights = {&light, 1}, .occluders = occluders}, view);
}
```

Call after drawing the world and outside camera/transform scopes. For several lights, collect them in one `LightFrame`
and draw the layer once. Use simulation positions for decisions and interpolated positions for rendering. Visual shadow
outlines may differ from collision blockers; neither light brightness nor softened edges decide AI visibility. Existing
[lighting and shadows](../../Graphics/Lighting.md#shadows) remain responsible for this rendering.

## Tests

From the engine repository: `python . test --label '^vision-cone$'`, then `python . check`.
Tests include only the facade, use explicit geometry/callbacks and the public collision API, and need no window.
Planned extensions are in [Roadmap](Roadmap.md).
