#pragma once
#include <cstddef>
#include <vector>
#include "Camera/Camera2D.h"
#include "Camera/CameraSettings.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/FloatRect.h"

// How Cameras::Split divides the canvas: side by side in columns, stacked in rows, or a 2 x 2 grid (three cameras: two
// above, one across the bottom; two: side by side).
enum class CameraSplit { SideBySide, Stacked, Grid };

// A scene's cameras, the place to ask "which camera?" (Unity's Camera.main): Main() is the one a single-screen game
// draws through and turns the mouse into the world with. Split gives each player a view of the same world.
//
//     Cameras _cameras{canvas, settings};                 // One camera over the whole canvas
//     _cameras.Main().Follow(player);  _cameras.Update(seconds);
//     for (const Camera2D& camera : _cameras) {          // One or several views
//         auto world = paint.Camera(camera.View());
//         DrawWorld(paint);
//     }
//     const Float2 aim = _cameras.Main().ToWorld(mouse);
class Cameras {
public:
    // Throws std::invalid_argument for settings that are not IsValid.
    explicit Cameras(const FloatRect& canvas, const CameraSettings& settings = {});

    [[nodiscard]] Camera2D& Main() { return _cameras.front(); }
    [[nodiscard]] const Camera2D& Main() const { return _cameras.front(); }
    // Throws std::out_of_range past Count().
    [[nodiscard]] Camera2D& operator[](std::size_t index) { return _cameras.at(index); }
    [[nodiscard]] const Camera2D& operator[](std::size_t index) const { return _cameras.at(index); }
    [[nodiscard]] std::size_t Count() const { return _cameras.size(); }
    [[nodiscard]] auto begin() { return _cameras.begin(); }
    [[nodiscard]] auto end() { return _cameras.end(); }
    [[nodiscard]] auto begin() const { return _cameras.begin(); }
    [[nodiscard]] auto end() const { return _cameras.end(); }

    // Gives 'count' cameras (1 to 4) a part of the canvas each, 'gap' canvas units apart. Cameras that exist keep where
    // they are and what they follow; new ones start as copies of the main camera. Throws std::invalid_argument outside
    // 1 to 4 or for a negative gap.
    void Split(std::size_t count, CameraSplit layout = CameraSplit::SideBySide, float gap = 0.0f);
    // The window changed size: each camera keeps its share.
    void SetCanvas(const FloatRect& canvas);
    [[nodiscard]] const FloatRect& Canvas() const { return _canvas; }

    void Update(float seconds);
    void Snap();
    // The camera whose viewport holds a canvas point, such as the mouse; null in a gap or outside the canvas.
    [[nodiscard]] const Camera2D* At(Float2 canvasPoint) const;

private:
    void Layout();
    FloatRect _canvas;
    CameraSplit _layout = CameraSplit::SideBySide;
    float _gap = 0.0f;
    std::vector<Camera2D> _cameras;
};
