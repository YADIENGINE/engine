#pragma once
#include <optional>
#include "Camera/CameraView.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Surface/Surface.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/FloatRect.h"
#include "Utilities/Geometry/Vec2.h"

// Where a PixelView's image goes for one frame (PixelView::Plan). Plain data.
struct PixelViewLayout {
    int scale = 1;     // Device pixels per art pixel: a whole number, at least 1
    Vec2 pixels{1, 1}; // The image's size in art pixels
    FloatRect area;    // The coordinates drawing inside the image uses: canvas units, the image's corner first
    FloatRect dest;    // Where the image lands on the target it is presented on, its corners on device pixels
    CameraView shown;  // The view to draw the world through: zoom on whole device pixels per art pixel, target on
                       // the art grid; the viewport is the one given
    Float2 stretch{1.0f, 1.0f}; // Presented units per unit inside (1 unless the canvas is scaled unevenly)

    // A point of the target it is presented on (the canvas) in the coordinates inside it, and back.
    [[nodiscard]] Float2 Inside(Float2 outside) const {
        return {area.x + (outside.x - dest.x) * stretch.x, area.y + (outside.y - dest.y) * stretch.y};
    }
    [[nodiscard]] Float2 Outside(Float2 inside) const {
        return {dest.x + (inside.x - area.x) / stretch.x, dest.y + (inside.y - area.y) / stretch.y};
    }
    // A canvas point (the mouse) in the world as shown, and back.
    [[nodiscard]] Float2 ToWorld(Float2 canvas) const { return shown.ToWorld(Inside(canvas)); }
    [[nodiscard]] Float2 ToCanvas(Float2 world) const { return Outside(shown.ToScreen(world)); }
};

// A pixel world for one camera view: the world is drawn into a small image whose pixel is one art pixel
// (Painter::ArtPixel canvas units), with hard edges, and the image is presented enlarged by a whole number of device
// pixels with nearest filtering, so every art pixel is the same size and nothing blurs. The camera moves smoothly: its
// target snaps to the art grid inside the image and the rest moves the whole image by device pixels. Zoom changes
// that whole number: the zoom shown is the nearest one where an art pixel covers whole device pixels, never less than
// one (a far zoom is the game's level of detail). Drawing inside stays in canvas coordinates, so a scene's passes and
// a LightLayer work unchanged; a Stage in a pixel project uses one per view (Docs/Graphics/PixelArt.md).
//
//     {
//         auto into = _pixels.Begin(paint, _camera.View());
//         auto world = paint.Camera(_pixels.Shown());
//         DrawWorld(paint);
//     }
//     _pixels.Present(paint);
//     const Float2 picked = _pixels.Layout().ToWorld(mouse); // What is under the mouse, as shown
//
// A view must not outlive the game's renderer (destroy it with the scene or view that owns it).
class PixelView {
public:
    PixelView();

    // Where the image goes for 'view' (its viewport, or 'area' without one) on a target whose logical rectangle is
    // 'area' at 'devicePerUnit' device pixels per unit, with 'artPixel' units per art pixel.
    [[nodiscard]] static PixelViewLayout Plan(const CameraView& view, const FloatRect& area, Float2 devicePerUnit,
                                              float artPixel);

    // Following drawing goes into the image for 'view' (the camera's view this frame, shake included), in canvas
    // coordinates, cleared first unless 'clear' is empty. Draw the world through Shown(). Call it outside paint.Camera
    // and paint.Transform, on the target it will be presented on (the canvas).
    [[nodiscard]] PaintScope Begin(Painter& paint, const CameraView& view,
                                   std::optional<Color4> clear = Color4{0, 0, 0, 0});
    // Draws the image where Begin placed it, clipped to the view's viewport; after Begin's scope ended, on the same
    // target and outside paint.Camera and paint.Transform.
    void Present(Painter& paint) const;

    // What the last Begin planned; Shown() is the view to draw the world through.
    [[nodiscard]] const PixelViewLayout& Layout() const { return _layout; }
    [[nodiscard]] const CameraView& Shown() const { return _layout.shown; }
    // The image itself (a pixel surface, Layout().pixels in size), for a presentation of one's own: a warped monitor.
    [[nodiscard]] const Surface& Image() const { return _image; }

private:
    Surface _image;
    PixelViewLayout _layout;
};
