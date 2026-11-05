#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>
#include "Camera/CameraView.h"
#include "Graphics/Draw/ParticleDraw.h"
#include "Graphics/Paint/PaintStyle.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Math/FloatRange.h"

class Painter;

// Draws one piece instead of its shape (the blend is already set). Draw only.
using DriftDraw = std::function<void(Painter&, const ParticleDrawData&)>;

// What one field of drifting things looks like: rain, snow, dust, or a game's own ash and leaves. Plain data.
struct DriftLook {
    float density = 0.0f;        // How many at full amount in 100 x 100 units; 0: none
    Float2 fall{};               // Their own movement, units/second
    float windShare = 1.0f;      // How much of the wind moves them
    FloatRange pace{1.0f, 1.0f}; // Each one's share of its movement
    FloatRange size{1.0f, 1.0f}; // Radius; a Spark's line width
    FloatRange spin{};           // Radians/second, clockwise, for shapes that show it
    float sway = 0.0f;           // How far each one swings sideways, in units
    float swayRate = 1.0f;       // Radians/second of that swing
    float twinkle = 0.0f;        // Share of the opacity that comes and goes, 0..1
    float streakSeconds = 0.0f;  // A Spark is as long as this many seconds of its movement
    float parallax = 1.0f;       // 1: stands in the world; 0: stuck to the screen; above 1: nearer than the world
    ParticleShape shape = ParticleShape::Circle;
    Color4 color{255, 255, 255, 255};
    Blend blend = Blend::Normal;
    DriftDraw draw{}; // Empty: 'shape'
};

// What DriftField accepts; its constructor throws invalid_argument otherwise.
[[nodiscard]] bool IsValid(const DriftLook& look);

// Things that fill the view and keep filling it. They live in one tile of the world that repeats without end, so a
// camera moving, zooming or turning never finds an edge, nothing is born or dies, and two views of a split screen share
// one field. The amount says how many of them show, so changing it adds and removes them one at a time, and a change of
// wind bends them all at once. View code: it decides nothing. For short bursts at a point, see ParticleSystem.
class DriftField {
public:
    explicit DriftField(const DriftLook& look, std::uint64_t seed = 1);

    // Each frame, on the clock that should move it: the game's stops it in a hitstop, a real one does not.
    void Update(float seconds, Float2 wind = {});
    // Call it outside paint.Camera and paint.Transform, with the view the world was drawn through; an empty view suits
    // a game without a camera. 'amount' is 0..1. With RenderQuality::backgroundEffects off half of them show, and a
    // view wider than one tile thins them out, so zooming far out does not cost more than one tile's worth.
    void Draw(Painter& paint, float amount, const CameraView& view = {}) const;

    // How many show at full amount, in one tile.
    [[nodiscard]] std::size_t Capacity() const { return _pieces.size(); }

private:
    struct Piece {
        Float2 at;
        Float2 velocity;
        float pace;
        float size;
        float phase;
        float swayPace;
        float rotation;
        float spin;
    };
    DriftLook _look;
    std::vector<Piece> _pieces;
};
