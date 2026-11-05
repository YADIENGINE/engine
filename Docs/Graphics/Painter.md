# Painter

The engine's drawing API. Commands are recorded without raylib, independently of the GPU, and submitted to the engine's
OpenGL 3.3 device at frame end. Design and decisions: [Render system](../Design/Rendering.md).

## Where it comes from

Each `GameObject` draws in `Draw(Painter& paint) const`; `Scene::Render`, `Game::Draw(Painter&)`, and
`Game::DrawWindow(Painter&)` pass the same Painter. Do not copy or retain Painter; use it only for the duration of the
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
| `Polygon(points, ShapeStyle)`, `Triangle` | Non-self-intersecting polygon (either winding direction) |
| `Mesh(points, colors, indices)` | Per-vertex colored triangles, color blends across each triangle (soft areas such as light masks); no antialiasing |
| `FillPath(path, ShapeStyle)`, `StrokePath(path, LineStyle)` | `Path`: `MoveTo`, `LineTo`, `QuadTo`, `CubicTo`, `ArcTo`, `Close` |
| `Glow(center, radius, RadialGlowStyle)` | Soft, additive decoration; follows `RenderQuality::glow` |
| `Light(LightSource, visible)` | A light that ignores the quality setting; see [Lighting](Lighting.md) |

`ShapeStyle` parts draw together, in order: halo (`glow`), fill (`fill` or `gradient`), border (`stroke`, `Inside` /
`Center` / `Outside`). Shortcuts: `Fill(color)`, `Outline(width, color)`. `Gradient::Linear(start, end, from, to)` is
local to the shape's center; `Gradient::Radial(radius, from, to)` starts at the center; `Gradient::Vertical(height, top,
bottom)`. Polygon gradients use the centre of the points' bounding box before the current transform; path fills use
the bounding box of all flattened contours in that same drawing coordinate system. Linear and radial gradients
include the antialiasing fringe. Polygon and path borders use `StrokeStyle::join` (`Miter` by default, `Bevel` or
`Round`); distance shapes retain their analytic borders.

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
fixed edges. `ImagePolygon(surface, points, sources)` maps part of a surface onto a convex polygon (broken mirror
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

`paint.Quality()` is the player's choice (`GlowQuality`, `BloomQuality`, `backgroundEffects`). Painter applies halos and
bloom itself; callers read it only to skip purely decorative work. `paint.Time()` / `paint.Delta()` are the presentation
clock (editor Pause freezes it); `paint.PixelScale()` gives device pixels per unit.

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
advances its own clock (e.g. a card in an editor scene) also calls the explicit-style function with `time`.

## Measurement

`Game::RenderStatistics()` gives the last frame's cost: draw calls, batches, vertices, uploaded bytes, state changes,
rasterized glyphs, reallocated surfaces. In the editor, **Engine > Render Gallery** shows all features on one screen.
