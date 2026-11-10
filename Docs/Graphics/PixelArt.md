# Pixel art

The engine's first-class look is pixel art drawn in code. A sprite is drawn once on a small grid of **inks**, frozen
into a `PixelSprite`, and shown with `Painter::Sprite` in any **palette**. Headers: `Core/Graphics/Pixel/`.
Example: [Pixel sprite recipe](../../AgentDocs/Recipes/PixelSprite.md).

"Pixel art" does not mean a low resolution: a sprite pixel can cover one screen pixel or several (`SpriteStyle::scale`).

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
paint.Sprite(sprite, palette, feet, {.scale = 3.0f, .flipX = facingLeft, .flash = White, .flashAmount = hurt});
```

Each sprite pixel covers `scale` units on both axes; whole numbers keep every pixel the same size. Without rotation in
the current transform the sprite's corners land on whole device pixels, so it never blurs and has no seams between
pixels. The renderer uploads a sprite the first time it is drawn (an atlas of inks) and each palette revision once.
A sprite has no rotation of its own.

## In the editor

A graphic asset whose registration sets `GraphicAsset::pixels` gets a pixel view in the Assets panel: **Pixels**
(zoomed to a whole number of screen pixels per sprite pixel), **Inks** and **Parts** (a colour per ink or part),
**Drawing** (as the game draws it), a grid, and the pivot. The Inspector shows the size, how many pixels each ink has,
and the pixel under the mouse. `python Engine asset new X --folder <Folder> --pixel` writes such an asset:
[Graphic assets](../Editor/Api.md#graphic-assets).
