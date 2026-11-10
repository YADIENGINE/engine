#pragma once
#include <optional>
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/Vec2.h"

// Where the Pixelator's canvas shows a sprite: a whole-number zoom (screen pixels per sprite pixel, so every pixel has
// the same size) and the screen position of the sprite's top-left corner, relative to the canvas area. Positions stay
// on whole screen pixels. A view that was fitted refits whenever the area or sprite size changes, until the developer
// zooms or pans.
class PixelatorView {
public:
    static constexpr int MinZoom = 1;
    static constexpr int MaxZoom = 64;
    static constexpr int GridFromZoom = 4; // A pixel grid needs pixels at least this many screen pixels wide

    [[nodiscard]] int Zoom() const { return _zoom; }
    [[nodiscard]] Float2 Origin() const { return _origin; }
    [[nodiscard]] bool Fitted() const { return _fitted; }
    // Refits a fitted view when the area or the sprite changed size; call once per frame.
    void Keep(Float2 area, Vec2 sprite);
    // The largest zoom at which the whole sprite fits (at least 1), centred.
    void Fit(Float2 area, Vec2 sprite);
    // Zoom 1, centred.
    void Actual(Float2 area, Vec2 sprite);
    // Changes the zoom by `steps` along 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, keeping the sprite point under
    // `point` (canvas-relative) where it is.
    void ZoomAt(Float2 point, int steps);
    // Sets the zoom (clamped), keeping the sprite point under `point` where it is.
    void SetZoom(int zoom, Float2 point);
    void Pan(Float2 delta);
    // The sprite pixel under a canvas-relative point, if the point is on the sprite.
    [[nodiscard]] std::optional<Vec2> PixelAt(Float2 point, Vec2 sprite) const;
    // Where a sprite pixel corner (x, y) is on the canvas.
    [[nodiscard]] Float2 ScreenOf(Float2 pixel) const;
    // Whether a pixel grid is drawn at this zoom.
    [[nodiscard]] bool GridVisible() const { return _zoom >= GridFromZoom; }
    // The zoom ladder's next step from `zoom` in the direction of `steps` (±1 per step).
    [[nodiscard]] static int StepZoom(int zoom, int steps);

private:
    void Centre(Float2 area, Vec2 sprite);
    int _zoom = 1;
    Float2 _origin;
    bool _fitted = true;
    Float2 _area;
    Vec2 _sprite;
};
