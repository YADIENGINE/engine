# Pixel art

The engine's first-class look is pixel art drawn in code. A sprite is drawn once on a small grid of **inks**, frozen
into a `PixelSprite`, and shown with `Painter::Sprite` in any **palette**. A pixel project draws its world into small
images whose pixel is one **art pixel**, enlarged to the window by a whole number. Headers: `Core/Graphics/Pixel/`
(the kit) and `Core/Graphics/PixelView/` (the pixel world). Examples:
[Pixel sprite recipe](../../AgentDocs/Recipes/PixelSprite.md), [Pixel world recipe](../../AgentDocs/Recipes/PixelWorld.md).

"Pixel art" does not mean a low resolution: an art pixel covers `artPixel` canvas units, and so several window pixels.

## Inks and palettes

A sprite stores inks, not colours. An `Ink` is an index into a `PixelPalette`; ink 0 (`ClearInk`) is always transparent.
Inks are numbered from 1 in the order the palette lists its colours, so a game names them with an enum:

```cpp
enum : Ink { Line = 1, Steel0, Steel1, Steel2, Steel3, Cloth0, Cloth1, Cloth2 };
const PixelPalette Guards{Rgb(0x14121c), Rgb(0x3a4152), Rgb(0x657088), Rgb(0x9aa6b8), Rgb(0xdfe6ee),
                          Rgb(0x4a2230), Rgb(0x7d3245), Rgb(0xb04a5c)};
```

A palette is a value with at most 255 colours. Copy it to make a variant: `Set` changes an ink (an enemy's own cloth),
`Mixed` moves every ink toward a colour (night, a fade). The same sprite then shows in each palette with no new pixels.
A hit flash needs no palette at all: `SpriteStyle::flash` and `flashAmount`.

An `InkRamp` names five inks of one material from dark to light (`deep`, `shade`, `base`, `lit`, `shine`); shaded shapes
use it. Inks may repeat in a ramp.

## Drawing on a canvas

A `PixelCanvas` is a grid of inks drawn in code. Draw shapes and let the canvas shade them rather than placing pixels one
by one; this is what keeps code-drawn pixel art good:

- **Shaded volumes:** `Poly`, `Ellipse`, `Limb` (a tapered capsule for arms, legs, tails) or any `PixelMask` with
  `Shade`. Pixels facing the light (`PixelCanvasStyle::light`, top left by default) get `lit` and `shine`, the far side
  `shade` and `deep`, with a dither between. `bevel` is how deep the lit and shaded bands go. `edged` first draws the
  edge ink where the shape meets what is already drawn, so a nearer part stands out from the one behind it.
- **Flat marks:** `FlatRect`, `FlatLine`, `FlatPoly`, `FlatEllipse`, `Fill`.
- **Details:** `Stamp` places a few pixels from strings (`'.'` keeps, digits and letters are inks through the canvas's
  `PixelLegend`); `Speckle` sprinkles one ink into another, the same way for the same seed; `Outline` rings the drawing.
- **Composition:** `Paste` another canvas, `FlipX`.

A pixel covers `x..x+1`; a shape takes the pixels whose centres it covers. Everything outside the grid is ignored and
everything is deterministic: the same code draws the same pixels.

**Parts.** `auto helmet = canvas.Part(MyPart::Helmet);` makes everything drawn until the scope ends belong to that part.
A sprite keeps its parts; they show in the editor's Parts view and are where a sprite will break.

## Sprites

`PixelSprite(canvas, pivot)` freezes a canvas. The pivot is the pixel corner that lands on the point the sprite is drawn
at, usually the middle of the feet: `{width / 2, height}`. A sprite is cheap to copy; copies share the pixels.

Painting a canvas costs more than drawing a sprite, so make each frame once: `PixelSpriteSet(count, &PaintFrame)` makes
sprite `i` the first time it is asked for. Number poses × frames however the game likes.

```cpp
paint.Sprite(sprite, palette, feet, {.flipX = facingLeft, .flash = White, .flashAmount = hurt});
```

**Animations.** A `PixelAnimation` lists which sprite shows for how long (`PixelAnimation::Even(count, fps)` for even
frames); `SpriteAt(time)` picks the sprite for a clock, looping or holding the last frame. The editor reads the same
list, so what it shows never drifts from what the game plays.

Each sprite pixel covers `scale` art pixels (`paint.ArtPixel()` canvas units each) on both axes; the default 1 makes a
sprite pixel one art pixel, and whole numbers keep every pixel the same size. In a smooth project an art pixel is one
canvas unit. Without rotation in the current transform the sprite's corners land on whole device pixels, so it never
blurs and has no seams between pixels. The renderer uploads a sprite the first time it is drawn (an atlas of inks) and
each palette revision once. A sprite has no rotation of its own.

## The pixel world

A project is a pixel project when `Game/Game.settings.json` sets `artPixel`, the canvas units one art pixel covers; new
projects use 3 (art 426×240 on the 1280×720 canvas). The canvas stays the coordinate system of logic, UI and the camera:
only how the world is drawn changes. `artPixel` 0 is a smooth project.

A `PixelView` draws one camera view's world into a pixel image and presents it:

```cpp
{
    auto into = _pixels.Begin(paint, _camera.View()); // Canvas coordinates inside, hard edges
    auto world = paint.Camera(_pixels.Shown());
    DrawWorld(paint);
}
_pixels.Present(paint);
```

- One pixel of the image is one art pixel, at every zoom: a sprite at scale 1 fills exactly one, and zoom never
  resamples it.
- The image is presented with nearest filtering at a whole number of window pixels per art pixel,
  `round(window pixels per canvas unit × artPixel × zoom)`, at least 1. That number comes from the real window, so a
  canvas scaled by a fraction (1366×768) still gets even pixels. The zoom shown is the one that number gives: zoom moves
  in steps, and a zoom below one window pixel per art pixel is the game's level of detail, not the view's.
- The camera moves smoothly: its target snaps to the art grid inside the image and the rest moves the whole image by
  window pixels (whole art pixels when an art pixel is one window pixel). The image is two pixels larger than the view for
  that.
- `Shown()` is the view actually drawn (zoom on its step, target on the grid); draw the world through it.
  `Layout().ToWorld(mouse)` turns a canvas point into the world exactly as shown, for picking.
- `Image()` is the pixel surface itself, for a presentation of one's own: an in-world monitor, a warped feed.

A [Stage](Stage.md#in-a-pixel-project) does all of this for each of its views in a pixel project. The HUD stays sharp on
the canvas by default (text in 720p units); `pixelUi` puts the stage's overlay and HUD into one more pixel view of the
whole canvas.

A pixel project always draws the canvas at the window's size and presents it with nearest filtering; the player's
render-height limit does not apply. The editor's viewport shows its frames with sharp pixels, enlarged by whole steps.

## Hard edges

Inside a pixel view or a pixel surface (`SurfaceOptions::pixels`) drawing has hard edges: circles, polygons, borders,
lines and text cover a pixel fully or not at all, so nothing blends with what is behind it and no in-between colours
appear at edges. A line thinner than a pixel is one pixel wide at full colour. Glows, lights and gradients stay smooth.
Surfaces opened inside a pixel view (a `LightLayer`'s image) take its resolution; a light layer by default has one light
value per art pixel there.

## In the editor

A graphic asset whose registration sets `GraphicAsset::pixels` opens in the **Pixelator**, a tab beside the Viewport
that comes to the front when the graphic is opened from Assets (or `ctl open`); the Viewport behind it shows the
graphic as the game draws it. `python Engine asset new X --folder <Folder> --pixel` writes such an asset:
[Graphic assets](../Editor/Api.md#graphic-assets).

The editor draws the pixels itself from the sprite data, so zoom and pan are instant at any size:

- **Canvas:** whole-number zoom 1–64× (wheel zooms around the pointer, `+` / `-`), pan with the middle button or
  Space + drag, **Fit** (`F`) and **1:1** (`1`). Pixels are always the same size.
- **Views:** **Color** (the palette), **Inks** and **Parts** (a distinct colour per ink or per part). A pixel grid
  (`G`, from 4× zoom), a tile grid every 8, 16 or 32 pixels, the pivot, and a checker, dark or light background.
- **Inks:** every ink the frame uses with its colour and pixel count. Hovering one shows only its pixels; clicking it
  keeps it isolated until clicked again (or Esc).
- **Frames:** play, pause and step (`,` / `.`) with each frame's duration; an item list switches between a set's items.
  The frames are the ones the registration declares, so they are exactly the game's: keep a graphic's frames as a
  `PixelSpriteSet` played by a `PixelAnimation` (sprite and seconds per frame), draw with `animation.SpriteAt(time)`,
  and register `GraphicPixels::Animated(sprites, animation, palette)` (`GraphicPixels::Still` for one sprite).
- **In game:** a small preview at a whole number of screen pixels per sprite pixel on the chosen background; a pixel
  project starts at its `artPixel`.
- **Status:** the size, zoom and pivot, and under the pointer x, y, ink, colour and part.

Agents see the same with `python Engine ctl pixels [--view inks] [--zoom N] [--frame N] [--tiles 16]`: a PNG and a
JSON summary (size, pivot, frames, inks with counts) ([Editor automation](../Editor/Automation.md#commands)).
