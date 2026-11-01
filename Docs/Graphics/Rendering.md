# Rendering

Drawing is recorded through [Painter](Painter.md). Raylib supplies the window, GL context, input, audio and
platform services; it does not draw game content.

`GameSettings::canvasWidth/canvasHeight` select logical coordinates (default 1280×720). Larger windows rasterize
the canvas at window resolution; smaller windows downscale the logical image. Aspect ratio is preserved and
the remaining window area uses `letterbox`. `smoothCanvas` selects linear filtering when presenting the canvas.

The runtime owns the frame. Scenes implement `Draw(Painter&)`; views receive the same Painter from their owner.
Window, renderer and GPU implementation headers are private. `CanvasReady` borrows the completed canvas for
viewport transport without transferring ownership. `DrawWindow(Painter&)` draws in window pixels after canvas
presentation; `DrawToolOverlay()` (tools only) renders native ToolUi on top after that drawing executed.

## Surfaces and scopes

`Surface` is an offscreen image with a logical size and renderer-managed pixel resolution. Draw inside
`auto target = paint.Target(surface)` and display it with `paint.Image(surface, destination)`.
`paint.Stale(surface)` detects when cached content must be redrawn. Surface owners must be destroyed before
the renderer. Use `paint.ImagePolygon` for convex textured fragments and `paint.Group` for whole-group opacity.

Transforms, clipping, blending and opacity are scoped and restore their previous state. Scopes close in reverse
order. `paint.Clip(area)` intersects the current clip; rotated clips are rejected. Camera transforms use
`CameraView::Transform()`. Glows use `paint.Glow`; batching and blend submission are automatic.

## Geometry and timing

| Header | Purpose |
| --- | --- |
| `Utilities/Float2.h`, `FloatRect.h`, `Affine2.h`, `Color4.h` | Continuous geometry, transformations and colors |
| `Graphics/Anim/Easing.h` | Curves, `Progress01`, `Since`, `Staggered`, `Crossed`, `CountDown` |
| `Utilities/MathUtils.h` | Constants, hashes, scalar interpolation and index helpers |
| `Utilities/UiTransform.h` | Centered aspect-preserving design-to-panel mapping and inverse input mapping |

```cpp
const UiTransform mapping(designArea, panelArea);
const Float2 local = mapping.Unmap(pointer);
auto region = paint.Transform(mapping.Affine());
auto clip = paint.Clip(designArea);
DrawWidget(paint);
```

Integer UiTransform results round with `std::lround`; design rectangle centers use integer division.
Invalid source or target sizes throw `std::invalid_argument`.
`Fx/ShatterPattern` provides headless convex Voronoi fragments and timed crack propagation.

## Instrumentation

`Game::RenderStatistics()` returns draw calls, batches, vertices, uploads, state changes, glyph rasterizations
and surface allocations. `FrameObserver` measures runtime stages. `Game::ConfigureFrameExecution` temporarily
changes FPS/VSync without saving player options. See [Profiler](../Editor/Profiler.md).
