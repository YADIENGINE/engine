#pragma once
#include <cstdint>
#include "Camera/CameraView.h"
#include "Utilities/Color4.h"

class Painter;

// What rain does to a floor seen from above. Plain data.
struct RippleLook {
    float density = 0.0f;  // How many are spreading at full amount in 100 x 100 units; 0: none
    float seconds = 0.45f; // How long each one lasts
    float radius = 9.0f;   // How far it spreads
    float squash = 1.0f;   // Its height against its width: below 1 for a floor seen at a slant
    float width = 1.0f;
    Color4 color{200, 215, 235, 120};
};

// What GroundRipples accepts; its constructor throws invalid_argument otherwise.
[[nodiscard]] bool IsValid(const RippleLook& look);

// Rings spreading anywhere on the ground. Each is computed from its index and the time, so nothing is kept per ring.
// The rain's streaks do not land on them, and on a floor no eye can tell. A world seen from the side has no floor to
// spread on, so it leaves them out. View code: it decides nothing.
class GroundRipples {
public:
    explicit GroundRipples(const RippleLook& look, std::uint64_t seed = 1);

    // Each frame, on the clock that should move it.
    void Update(float seconds);
    // Call it outside paint.Camera and paint.Transform, with the view the world was drawn through. 'amount' is 0..1.
    // With RenderQuality::backgroundEffects off half of them show.
    void Draw(Painter& paint, float amount, const CameraView& view = {}) const;

private:
    RippleLook _look;
    unsigned _seed;
    unsigned _count = 0;
    double _time = 0.0;
};
