# Painter

The engine's drawing API. Commands are recorded without raylib, independently of the GPU, and submitted to the engine's
OpenGL 3.3 device at frame end. Frame and surface ownership: [Rendering](Rendering.md).

## Where it comes from

A scene draws in `Draw(Painter& paint) const` and hands the Painter to its views; `Scene::Render`, `Game::Draw(Painter&)`,
and `Game::DrawWindow(Painter&)` pass the same Painter. Do not copy or retain Painter; use it only for the duration of the
call.

```cpp
#include "Graphics/Paint/Painter.h"

void ScoreView::Draw(Painter& paint) const {
    paint.RoundRect(_panel, 8.0f, {.fill = Rgb(0x141826), .stroke = {2.0f, Rgb(0x30e0ff)}});
    paint.Text(_font, "SCORE", _panel.Center(), {.size = 24.0f, .align = TextAlign::Center});
}
```

Coordinates are logical canvas units (y down); angles are radians, clockwise on screen. Everything draws at float
positions with antialiasing; submission waits until frame end, but call order is always preserved.

## Basic types

Under `Utilities/Geometry/`, headless: `Float2` (operations, `Length`, `Normalized`, `Rotated`,
`Float2::FromAngle` ...), `FloatRect` (`FromCenter`, `Center`, `Inset`, `Intersect`, `Union` ...), and `Affine2`
(`Translate`, `Rotate`, `Scale`, `Camera`, `*`, `Inverse`). `Utilities/Color4.h` provides straight-alpha color with
`Rgb(0xRRGGBB)`, `Hsv(hue, s, v)`, `WithAlpha`, `ScaleAlpha` and `Mix`.

## Shapes

| Call | Note |
| --- | --- |
| `Rect`, `RoundRect(rect, radius)`, `ChamferRect(rect, chamfer)` | Chamfered rectangle: neon panels |
| `Circle(center, radius)`, `Ellipse(center, radii)` | |
| `Arc(center, radius, start, sweep, LineStyle)` | `Butt` / `Round` caps; a full turn draws a ring |
| `Line(from, to, LineStyle)` | `Butt` / `Round` / `Square` caps |
| `Polyline(points, LineStyle, closed)` | `Miter` (`Bevel` beyond the limit) / `Bevel` / `Round` joins |
| `Polygon(points, ShapeStyle)`, `Triangle` | Any polygon, concave included, either winding direction |
| `Mesh(points, colors, indices)` | Per-vertex colored triangles, color blends across each triangle (soft areas such as light masks); no antialiasing |
| `FillPath(path, ShapeStyle)`, `StrokePath(path, LineStyle)` | `Path`: `MoveTo`, `LineTo`, `QuadTo`, `CubicTo`, `ArcTo`, `Close`; holes: [below](#holes-and-fill-rules) |
| `Glow(center, radius, RadialGlowStyle)` | Soft, additive decoration; follows `RenderQuality::glow` |
| `Light(LightSource, visible)` | A light that ignores the quality setting; see [Lighting](Lighting.md) |

`ShapeStyle` parts draw together, in order: halo (`glow`), fill (`fill` or `gradient`), border (`stroke`, `Inside` /
`Center` / `Outside`). Shortcuts: `Fill(color)`, `Outline(width, color)`. `Gradient::Linear(start, end, from, to)` is
local to the shape's center; `Gradient::Radial(radius, from, to)` starts at the center; `Gradient::Vertical(height, top,
bottom)`. Polygon gradients use the centre of the points' bounding box before the current transform; path fills use
the bounding box of all flattened contours in that same drawing coordinate system. Linear and radial gradients
include the antialiasing fringe. Polygon and path borders use `StrokeStyle::join` (`Miter` by default, `Bevel` or
`Round`) and every `StrokeAlign`; distance shapes retain their analytic borders. A polygon's or path's glow is rounded
at the corners and fades with the distance from the edge.

### Holes and fill rules

`FillPath` fills the area a path's contours enclose. The path's `FillRule` decides what is inside:

- `NonZero` (default, as in SVG and Canvas): overlapping contours merge into one shape; a contour running the other
  way (points in reverse order, `ArcTo` with a negative sweep) cuts a hole.
- `EvenOdd`: any contour inside another cuts a hole, whatever its direction; a contour inside that hole fills again.

```cpp
Path window;
window.SetFillRule(FillRule::EvenOdd);
window.AddPolygon(wall);   // The building's outline
window.AddPolygon(pane);   // Becomes a hole
paint.FillPath(window, {.fill = brick, .stroke = {2.0f, mortar, StrokeAlign::Inside}});
```

The border and glow of a filled path follow the area's outline: two overlapping circles get one border around both.
`StrokePath` draws each contour as it was built instead. `Polygon` skips this work: it fills its points as they are,
so a self-intersecting outline gets a reasonable fill rather than one decided by a rule.

Lines thinner than one pixel fade instead of becoming thinner. Zero sizes or NaN positions do not draw.

Programmer errors (negative widths or radii, mismatched spans, a clip under a rotating transform, scopes ended out
of order) throw in Debug builds: `invalid_argument` at the call, `logic_error` for scopes when the frame ends. Release
builds never throw for them: the call draws nothing (a rotated clip uses its bounding box), the frame goes on,
`RenderStats::paintErrors` counts it and the first message is written to stderr. Drawing helpers report their own
invalid arguments the same way with `paint.Reject("...")`. The game's own build decides, not the engine libraries'
(`Diagnostics/DevelopmentChecks.h`).

## Images and surfaces

`Surface` is an offscreen drawing target: its size is logical, its resolution follows the canvas. Draw into it with
`paint.Target(surface)`, draw it with `paint.Image(surface, dest, style)`. Content is lost on scale changes; to draw
expensive, static content once:

```cpp
if (paint.Stale(_background)) {
    auto into = paint.Target(_background);
    DrawMapFloor(paint);
}
paint.Image(_background, field);
```

`ImageStyle`: source region, rotation around the center, tint. `NineSlice(texture, dest, style)` draws a panel with
fixed edges. `BlurredImage(surface, dest, along)` draws a surface blurred along one axis; two of them crossing, through a second surface, are a full blur. `ImagePolygon(surface, points, sources)` maps part of a surface onto a convex polygon (broken mirror
pieces, rippling areas); sources use the surface's logical units. A Surface must not outlive the game's renderer (its
owner is a scene / View).

## Text

Text without an explicit font uses the game's font: `paint.Font()` (`GameSettings::font`, same object as
`Game::Font()`); `FontFace::FromFile` loads other fonts, drawn with `paint.Text(font, ...)`. Glyphs rasterize on first
use at their on-screen pixel size: every size is sharp and all characters in the font are drawable (missing characters
become `?`).

```cpp
paint.Text("Ready", {800.0f, 120.0f}, {.size = 48.0f, .align = TextAlign::Center, .glow = {10.0f, cyan}});
paint.TextBox(description, area, {.size = 18.0f, .wrap = TextWrap::Word});
const TextBlock block = paint.Font().Layout(longText, style); // Lay out once and draw every frame
```

Position is the top of the first line; horizontally, the line's left edge, center, or right edge according to `align`.
`TextStyle`: size, color, alignment, `maxWidth` + `TextWrap::Word` / `TextFit::Shrink`, line spacing (`lineSpacing` as a
multiple of the font's line height or `lineHeight` in units), letter spacing, `glow`, `outline`, `shadow`, `emission`.
Common label shortcuts: `CenteredText(size, color, maxWidth)`, `FittedText(size, color, maxWidth, align)`. `Text` /
`TextBox` return the drawn block size; stacked paragraphs advance by it. Text reduced by `TextFit::Shrink` stays
vertically centered in the line at the requested size. `font.Measure(text, size)` measures without wrapping;
`font.FittedSize(text, size, maxWidth)` gives the size Shrink would use (for hit testing and adjacent text). Identical
text is not measured again each frame (layout cache). Iterate UTF-8 characters with `Utilities/Utf8.h`
(`NextCodepoint`).

## Scopes

State changes are always scoped; destroying the scope object restores the previous state:

```cpp
{
    auto moved = paint.Transform(Affine2::Rotate(angle, center)); // Multiplied by the current transform
    auto clip = paint.Clip(area);       // Intersects current clip; an error under a rotated transform
    auto faded = paint.Opacity(0.5f);   // Cheap; overlapping areas darken
    auto light = paint.Blending(Blend::Add);
    auto group = paint.Group({.opacity = 0.5f}); // Draws to a temporary image first, composites as one piece
    auto into = paint.Target(surface);  // Transform / clip / opacity start fresh inside
    auto mark = paint.Marker("HUD");    // Region name in RenderDoc (literal)
}
```

A scope can be moved; the moved-from object ends nothing. A conditional scope is a `std::optional<PaintScope>` filled
with `emplace` (assignment is deleted), and a helper may return a scope:

```cpp
std::optional<PaintScope> into;
if (layered) into.emplace(paint.Target(layer));
DrawField(paint); // Into the layer or straight onto the canvas
```

Scopes still end in reverse order; a moved scope outliving a later one is reported when the frame ends.

`Blend::Normal` and `Blend::Add` share a draw call (premultiplied alpha); `Multiply` / `Screen` split the batch.
`Multiply` changes colour only: what feeds bloom stays.

## Post-process and quality

`GameSettings::post` is the game's default; `paint.Post()` changes it for that frame (the next frame returns to the
default): bloom (`intensity`, `radius`), `vignette`, `scanlines`, `aberration`. Bloom comes only from drawing with
`emission` > 0 (`ShapeStyle::emission`, `LineStyle::emission`, `TextStyle::emission` ...). When a frame first requests
bloom, the canvas retains the bloom layer starting with the next frame.

`paint.Quality()` is the player's choice (`GlowQuality`, `BloomQuality`, `backgroundEffects`, `screenMotion`). Painter applies halos and
bloom itself; callers read it only to skip purely decorative work. `paint.Time()` / `paint.Delta()` are the presentation
clock (editor Pause freezes it); `paint.PixelScale()` gives device pixels per unit.

### Analog finish

`PostSettings::region` limits the finish to one canvas-space rectangle, for example the face of an in-world monitor:
`paint.Post().region = {40.0f, 20.0f, 320.0f, 150.0f};`. Empty means the full canvas. The rectangle is clipped to the
canvas; everything outside passes through unchanged. Effects use coordinates local to the region, while distances
remain canvas units and `scanlineCount` spans its height. `Mix` selects the first region before halfway and the second
from halfway onward. Bloom is still gathered across the canvas, so light from outside the region can reach inside it.

More fields of `PostSettings`, each usable alone and free at 0:

| Field | What it does | A place to start |
|---|---|---|
| `grain` (`amount`, `size`) | Film grain that never repeats; stronger in the dark than on bright things | `amount` 0.06 for a constant texture, 0.3 for a heavy one |
| `fade` | Drains colour toward grey | 0.3 to 0.7 |
| `tint` (`color`, `amount`) | Multiplies a colour over the image; a dark, cold colour gives a dark, cold look | `Rgb(0x8c9ccc)` at 0.8 |
| `wobble` (`amount`, `speed`) | Bends and tilts the image slowly | `amount` 3 is uneasy, 12 is hard to read |
| `tape` (`band`, `shift`, `speed`) | A tracking band that rolls down the image, and lines that jump sideways | `band` 0.5; `shift` up to 2 keeps text readable |

By default they cover what is on the canvas, the HUD too, so readability limits them. Wobble fades out toward the edges,
so a HUD along them stays still. Grain is weak on bright text. Check small text in the middle of the screen, where the wobble is
strongest, and text in the corners with vignette and tint together: they multiply.

`Mix(calm, disturbed, amount)` gives the finish between two others, so one number can drive all of it:
`paint.Post() = Mix(calm, disturbed, danger);`. Drive that number from what the game knows, or with a `SecondsTween<float>`
for a shock that fades. `SecondsTween<PostSettings>`, clips and timelines also work directly, through the same `Mix`. An
effect that is off on one side keeps the other side's colour, size and speed. A speed may change at any moment without a
jump, because the renderer adds the rhythm up frame by frame.

**The player's switch:** `RenderQuality::screenMotion` (saved with the options; a row in `UiOptionsScreen`) turns grain,
wobble and tape off. Fade, tint, vignette and scanlines stay, since they do not move. The renderer applies it; a game does
not check it, and needs no option of its own. If a game's own options screen does not use `UiOptionsScreen`, it should
offer this row.

All of this is view code on the presentation clock (`paint.Time()`), so pausing holds the image, and a hitstop does not. It
uses no `Rng` and never reaches a simulation. Rewinding in the editor leaves the wobble and the tape where they are, so the
same second can look different. An example: [recipe](../../AgentDocs/Recipes/AnalogFinish.md); every effect alone:
the editor catalog entry `engine.post.gallery`. A burst that ends by itself is `FxStage::Glitch`, drawn on the canvas.

## Helpers and theme

`DrawBand` draws an arc between inner/outer radii with a `LineStyle` (its width is replaced); `DrawRing` uses a `ShapeStyle` with the same radius/width convention.
`Path::AddRegularPolygon` adds a closed contour with the same clockwise corners as `DrawRegularPolygon`; fewer than three sides add nothing.

Shared shapes and themed widgets sit above Painter; all take `Painter&` first, read quality and time from Painter, and
draw text with the game's font:

| Header | Contents |
| --- | --- |
| `Graphics/Draw/DrawUtils.h` | `DrawStar`, `DrawDiamond`, `DrawRegularPolygon`, `DrawRotatedRect`, `DrawLightning`, `DrawTabOutline`, `DrawTabbedPanelOutline`, `RowSlot` |
| `Graphics/Draw/Neon.h` | Explicit-style neon shapes and styles: `DrawNeonFrame/Text/Panel/Chip/Ring`, `DrawArcadeButton(Body)`, `DrawRoundButton`, `DrawKeyCap`, `DrawProgressBar` |
| `Graphics/Draw/BackgroundDraw.h` | `DrawGrid2D`, `DrawScanlines`, `DrawVignette`, `DrawBackdrop*` with `BackdropStyle` |
| `Graphics/Draw/Elevation.h` | Height in a 2.5D world: `Raised` (a scope that lifts what follows) and `DrawGroundShadow` with `GroundShadow` ([Stage](Stage.md#depth)) |
| `Graphics/Backgrounds/Backdrops.h` | Theme background roles: `DrawBackdropGrid(paint, BackdropRole::Menu, area)` ... |
| `Graphics/Backgrounds/` scenes | Whole-screen scenes with their own look: [ready-made scenes](ThemesAndUi.md#ready-made-scenes) |
| `Graphics/DefaultIcons.h` | Shared icon set in its own colors: `DrawIcon(paint, Icon::Star, center, size)`; override with `{.color = gold}` |
| `Graphics/KeyPrompts.h` | Keyboard keys, WASD and arrow sets, mouse buttons for control hints: `DrawKeyPrompt(paint, box, KeyPrompt::Space)`. Labels are strokes, so a game needs no font; `KeyPromptAspect` lays out a row |
| `Fx/*` | Effects (particles, floating text, rings, shockwaves, overlays, cameras, `FxStage`): [Effects](Effects.md) |

A camera's view is `Camera/CameraView.h` (`offset`, `target`, `rotation` in radians, `zoom`, `viewport`; `Transform()`,
`ToScreen`, `ToWorld`, `VisibleArea`); draw the world within `auto world = paint.Camera(view)`, which also clips to the
view's viewport. A game camera that follows makes the view: [Camera](../Camera.md).

Menus and widgets are not drawn by hand: see [UI](../Ui.md).

One-off variant: copy `CurrentTheme().ui.X`, modify it, and pass it to the explicit-style function. Drawing that
advances its own clock (e.g. a card in a catalog scene) also calls the explicit-style function with `time`.

## Measurement

`Game::RenderStatistics()` gives the last frame's cost: draw calls, batches, vertices, uploaded bytes, state changes,
rasterized glyphs, reallocated surfaces. In the editor, **Engine > Render Gallery** shows all features on one screen.
