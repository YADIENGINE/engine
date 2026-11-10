Show a pixel-art world: tiles and a sprite drawn in art pixels, a following camera that scrolls smoothly, a sharp HUD and
mouse picking.

APIs: [Stage.h](../../Core/Stage/Stage.h) (`Shown`, `ToWorld`), [PixelView.h](../../Core/Graphics/PixelView/PixelView.h),
[Painter.h](../../Core/Graphics/Paint/Painter.h) (`ArtPixel`, `Sprite`). Concepts:
[Pixel art](../../Docs/Graphics/PixelArt.md#the-pixel-world), [Stage](../../Docs/Graphics/Stage.md#in-a-pixel-project).

The project makes it pixel art, not the code: `Game/Game.settings.json` sets `"artPixel": 3` (new projects do). The stage
then draws each view through a pixel view; the scene draws in canvas coordinates as always. Sizes of pixel art are art
pixels times `paint.ArtPixel()`; a sprite at scale 1 is one art pixel per pixel. Pick with `Stage::ToWorld`, which
knows the zoom step and the snapped camera. Without a stage, use a `PixelView` directly
([Pixel art](../../Docs/Graphics/PixelArt.md#the-pixel-world)). Sprites themselves: [Pixel sprite recipe](PixelSprite.md).

<!-- recipe:PixelWorld-header -->
```cpp
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
```
<!-- /recipe -->

<!-- recipe:PixelWorld -->
```cpp
#include "AgentDocs/Recipes/Code/PixelWorld.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Pixel/PixelCanvas.h"

namespace {
constexpr int TileArt = 16;      // A floor tile, in art pixels
constexpr float WalkArt = 48.0f; // Art pixels per second
constexpr int MostTiles = 64;    // Per axis on screen; far beyond any view
constexpr Color4 FloorA = Rgb(0x2b2f3a);
constexpr Color4 FloorB = Rgb(0x323746);
constexpr Color4 Pick = Rgb(0xf2c14e);

enum : Ink { Line = 1, Dark, Mid, Light, Shine };
constexpr InkRamp Body{Dark, Dark, Mid, Light, Shine};

PixelSprite PaintWalker() {
    PixelCanvas c(12, 16); // Light from the top left
    c.Ellipse(6.0f, 9.0f, 5.0f, 6.0f, Body);
    c.Stamp(4, 7, {"1.1"});
    c.Outline(Line);
    return PixelSprite(c, {6, 16}); // Feet at the bottom middle
}
} // namespace

RecipePixelWorld::RecipePixelWorld(const SceneServices& services)
    : _stage(services, {.camera = {.follow = {.smoothTime = 0.25f}}})
    , _walker(PaintWalker())
    , _palette{Rgb(0x14121c), Rgb(0x2f4a6b), Rgb(0x4d86a8), Rgb(0x8ccbd9), Rgb(0xf2f0e6)} {
    _stage.Cameras().Main().Follow(_at);
    _stage.Cameras().Snap(); // Start on the walker
}

void RecipePixelWorld::Update(float seconds, Float2 move, Float2 mouse) {
    _at = _at + move * (WalkArt * _art * seconds); // The world is in canvas units; any position is fine
    _stage.Cameras().Main().Follow(_at);
    _stage.Update(seconds);
    // What is under the mouse in the world as the last frame showed it (zoom on its step, camera on the art grid).
    const Float2 world = _stage.ToWorld(mouse);
    const float tile = static_cast<float>(TileArt) * _art;
    _picked = Vec2{static_cast<int>(std::floor(world.x / tile)), static_cast<int>(std::floor(world.y / tile))};
}

void RecipePixelWorld::DrawWorld(Painter& paint, const StagePass& pass) const {
    _art = paint.ArtPixel();
    const float tile = static_cast<float>(TileArt) * _art;
    const FloatRect seen = pass.view.VisibleArea();
    const int left = static_cast<int>(std::floor(seen.x / tile));
    const int top = static_cast<int>(std::floor(seen.y / tile));
    const int columns = std::min(static_cast<int>(std::ceil(seen.width / tile)) + 1, MostTiles);
    const int rows = std::min(static_cast<int>(std::ceil(seen.height / tile)) + 1, MostTiles);
    for (int y = top; y < top + rows; ++y)
        for (int x = left; x < left + columns; ++x)
            paint.Rect({static_cast<float>(x) * tile, static_cast<float>(y) * tile, tile, tile},
                       Fill((x + y) % 2 == 0 ? FloorA : FloorB));
    if (_picked) // A border one art pixel wide, inside the tile
        paint.Rect({static_cast<float>(_picked->x) * tile, static_cast<float>(_picked->y) * tile, tile, tile},
                   Outline(_art, Pick, StrokeAlign::Inside));
    paint.Sprite(_walker, _palette, _at); // One sprite pixel per art pixel
}

void RecipePixelWorld::DrawHud(Painter& paint) const {
    // Sharp on the canvas in a pixel project (unless pixelUi): a frame around the screen, in canvas units.
    paint.Rect(paint.Bounds(), Outline(4.0f, Pick, StrokeAlign::Inside));
}
```
<!-- /recipe -->
