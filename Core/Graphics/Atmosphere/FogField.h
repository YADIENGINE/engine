#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include "Camera/CameraView.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"

class Painter;

// Where fog stands in a world seen from the side: world heights, top above bottom, fading over 'soft' at both ends.
struct FogBand {
    float top = 0.0f;
    float bottom = 0.0f;
    float soft = 0.0f;
};

// What one sheet of fog looks like. Plain data.
struct FogLook {
    Color4 color{200, 205, 215, 255}; // Alpha: how thick its thickest parts are
    float wavelength = 520.0f;        // Size of its largest patches, in units
    Float2 drift{};                   // Its own movement, units/second
    float windShare = 0.6f;           // How much of the wind moves it
    float boil = 8.0f;                // How fast its shape changes in still air, units/second
    float cell = 28.0f;               // Canvas units between the points it is computed at; lower costs more
    float parallax = 1.0f;            // 1: stands in the world; 0: stuck to the screen; above 1: nearer than the world
    std::optional<FogBand> band{};    // Empty: everywhere, for a world seen from above
};

// What FogField accepts; its constructor throws invalid_argument otherwise.
[[nodiscard]] bool IsValid(const FogLook& look);

// Fog that drifts and changes shape and never shows a seam or a repeat: smooth noise computed at a coarse grid over the
// view each frame, and blended between its points. The amount thickens existing patches and then joins them. Drawn
// before a LightLayer it shows only where light falls. View code: it decides nothing.
class FogField {
public:
    explicit FogField(const FogLook& look, std::uint64_t seed = 1);

    // Each frame, on the clock that should move it.
    void Update(float seconds, Float2 wind = {});
    // Call it outside paint.Camera and paint.Transform, with the view the world was drawn through; an empty view suits
    // a game without a camera. 'amount' is 0..1. With RenderQuality::backgroundEffects off its grid is twice as coarse.
    void Draw(Painter& paint, float amount, const CameraView& view = {}) const;

private:
    static constexpr std::size_t Octaves = 3;
    FogLook _look;
    unsigned _seed;
    std::array<Float2, Octaves> _offsets{};
    mutable std::vector<Float2> _points; // Hold nothing between frames; only their memory is kept
    mutable std::vector<Color4> _colors;
    mutable std::vector<std::uint32_t> _indices;
    mutable int _columns = 0;
    mutable int _rows = 0;
};
