// [recipe:PixelWorld-header]
#pragma once
#include <optional>
#include "Graphics/Pixel/PixelPalette.h"
#include "Graphics/Pixel/PixelSprite.h"
#include "Stage/Stage.h"
#include "Utilities/Geometry/Vec2.h"

// A pixel world: a tiled floor and a walking sprite, seen through a camera that follows it. In a pixel project
// (Game.settings.json "artPixel": 3) the stage draws each view into a pixel view: one sprite pixel is one art pixel,
// edges are hard and the camera scrolls smoothly; the HUD stays sharp. The mouse picks the tile under it, as shown.
class RecipePixelWorld final : public StageView {
public:
    explicit RecipePixelWorld(const SceneServices& services);
    // Each frame, from the scene's two hooks: real time, and game time with how the walker moves (-1..1 per axis) and
    // where the mouse is on the canvas.
    void UpdateReal(float realSeconds) { _stage.UpdateReal(realSeconds); }
    void Update(float seconds, Float2 move, Float2 mouse);
    void Draw(Painter& paint) const { _stage.Draw(paint, *this); }
    // The floor tile under the mouse in the frame last shown.
    [[nodiscard]] std::optional<Vec2> Picked() const { return _picked; }

private:
    void DrawWorld(Painter& paint, const StagePass& pass) const override;
    void DrawHud(Painter& paint) const override;

    Stage _stage;
    PixelSprite _walker;
    PixelPalette _palette;
    Float2 _at;
    std::optional<Vec2> _picked;
    mutable float _art = 1.0f; // Canvas units per art pixel, from the last frame (Painter::ArtPixel)
};
// [/recipe]
