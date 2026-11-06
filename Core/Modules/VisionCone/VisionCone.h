#pragma once
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <vector>
#include "Physics/Collision/CollisionWorld.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Math/Scalar.h"

// A closed circular sector in world coordinates (positive Y down). Angles are clockwise radians.
struct VisionConeSpec {
    Float2 origin{};
    float direction = 0.0f;
    float range = 100.0f;        // Zero disables sight; negative is invalid
    float halfAngle = Pi / 4.0f; // (0, Pi]; Pi sees a full circle
};

// First blocker along the closed segment [from, to], as a finite fraction in [0, 1]; nullopt means clear.
// Must also accept zero-length segments. Called synchronously, never retained by a query or Outline.
using VisionTrace = std::function<std::optional<float>(Float2 from, Float2 to)>;

enum class VisionStatus { Visible, OutOfRange, OutsideAngle, Occluded };

// Geometric rejection takes precedence over occlusion. Distance is in world units, even on rejection.
struct VisionPointResult {
    VisionStatus status = VisionStatus::OutOfRange;
    double distance = 0.0;
    std::optional<float> blockingFraction; // Present only for Occluded; 0 and 1 both block
};

// Counts explicit target samples, including points outside the cone in total. Empty input gives three zeros.
struct VisionSampleResult {
    std::size_t total = 0;
    std::size_t inCone = 0;
    std::size_t visible = 0;
};

// Uniform segments across the entire cone arc. At least 1 for a sector, or 3 for a full circle.
struct VisionOutlineOptions {
    int segments = 64;
};

// Opt-in, CPU-only sight queries and approximate drawing contours. No entity ownership, clock or AI state.
// Invalid specs/points/options/trace fractions throw std::invalid_argument; callback exceptions propagate.
// Independent queries may run concurrently if their callbacks permit it. Outline uses instance-owned scratch memory;
// do not call it concurrently or reenter it on the same instance (queries from its callback are fine).
class VisionCone final {
public:
    // Range and angle boundaries are included. The origin passes the angle test; range zero contains nothing.
    [[nodiscard]] static bool Contains(const VisionConeSpec& spec, Float2 point);
    // Empty trace: no obstacles. Out-of-range/out-of-angle points never invoke the trace.
    [[nodiscard]] static VisionPointResult Query(const VisionConeSpec& spec, Float2 point,
                                                 const VisionTrace& trace = {});
    // Each sample is queried in input order. The caller decides what visible / total means for its AI.
    [[nodiscard]] static VisionSampleResult QuerySamples(const VisionConeSpec& spec, std::span<const Float2> points,
                                                         const VisionTrace& trace = {});
    // Borrows world, which must outlive every use of the returned callback. Mask/ignore are copied; live world changes
    // are observed. Use a blocker mask excluding target colliders, and ignore the observer's collider when needed.
    [[nodiscard]] static VisionTrace CollisionTrace(const CollisionWorld& world, CollisionMask blockers,
                                                    ColliderId ignore = {});
    // Clockwise polygon: origin then both arc ends for sectors; perimeter only for full circles. No repeated closing
    // vertex; consecutive duplicates are removed. Zero range or fewer than three vertices returns empty.
    // Blocked samples stop at the first hit. Narrow obstacles between rays can be
    // missed: use Query for gameplay. Coordinates/endpoints must be representable as finite Float2 values.
    // Borrowed until the next Outline call (even if it throws), assignment, move or destruction of this instance.
    [[nodiscard]] std::span<const Float2> Outline(const VisionConeSpec& spec, const VisionOutlineOptions& options = {},
                                                  const VisionTrace& trace = {});

private:
    std::vector<Float2> _outline;
};
