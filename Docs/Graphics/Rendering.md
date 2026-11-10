# Rendering

Drawing is recorded through [Painter](Painter.md). Raylib supplies the window, GL context, input, audio and
platform services; it does not draw game content.

The project settings' `canvasWidth` / `canvasHeight` ([Project settings](../ProjectSettings.md)) select logical
coordinates (default 1280×720). Larger windows rasterize
the canvas at window resolution; smaller windows downscale the logical image. Aspect ratio is preserved and
the remaining window area uses `letterbox`. A pixel project (`artPixel` above 0) always rasterizes the canvas at the
window's size, ignores the player's render-height limit and presents it with nearest filtering; its world is drawn
through pixel views ([Pixel art](PixelArt.md#the-pixel-world)).

The runtime owns the frame. Scenes implement `Draw(Painter&)`; views receive the same Painter from their owner.
Window, renderer and GPU implementation headers are private. `CanvasReady` borrows the completed canvas for
viewport transport without transferring ownership. `DrawWindow(Painter&)` draws in window pixels after canvas
presentation; `DrawToolOverlay()` (tools only) renders native ToolUi on top after that drawing executed.

## Surfaces and scopes

`Surface` is an offscreen image with a logical size and renderer-managed pixel resolution: the resolution of the
target it is drawn from (the canvas's, or a pixel view's inside one). Draw inside
`auto target = paint.Target(surface)` and display it with `paint.Image(surface, destination)`;
`paint.Target(surface, area)` makes a logical rectangle fill the image instead. `SurfaceOptions::pixels` makes a pixel
surface: its size is in pixels, it is nearest filtered and drawing in it has hard edges.
`paint.Stale(surface)` detects when cached content must be redrawn. Surface owners must be destroyed before
the renderer. Use `paint.ImagePolygon` for convex textured fragments and `paint.Group` for whole-group opacity.

Transforms, clipping, blending and opacity are scoped and restore their previous state. Scopes close in reverse
order. `paint.Clip(area)` intersects the current clip; rotated clips are rejected. Camera transforms use
`CameraView::Transform()`. Glows use `paint.Glow`; batching and blend submission are automatic.

## Geometry and timing

| Header | Purpose |
| --- | --- |
| `Utilities/Geometry/Float2.h`, `Utilities/Geometry/FloatRect.h`, `Utilities/Geometry/Affine2.h`, `Utilities/Color4.h` | Continuous geometry, transformations and colors |
| `Animation/Easing.h` | Curves, `Progress01`, `Since`, `Staggered`, `Crossed`, `CountDown` ([Animation](../Animation.md)) |
| `Utilities/Math/Scalar.h` | Constants, angle conversion, fractional parts and scalar interpolation |
| `Utilities/Random/Noise.h` | Stable hashes for decorations (`Hash`, `Random01`, `NoiseHash`) |
| `Utilities/Random/NoiseField.h` | Smooth noise: `Noise(settings, point)` over a plane or along a line, `Warp`; kinds Simplex, SimplexSmooth, Cellular, Perlin, Value; FBm, Ridged and PingPong layers. Same settings and point, same value; usable in simulations |
| `Utilities/Math/Index.h` | Wrapped indices |
| `Ui/Layout/UiTransform.h` | Centered aspect-preserving design-to-panel mapping and inverse input mapping |

```cpp
const UiTransform mapping(designArea, panelArea);
const Float2 local = mapping.Unmap(pointer);
auto region = paint.Transform(mapping.Affine());
auto clip = paint.Clip(designArea);
DrawWidget(paint);
```

Integer UiTransform results round with `std::lround`; design rectangle centers use integer division.
Invalid source or target sizes throw `std::invalid_argument`.
`Utilities/Geometry/ShatterPattern.h` provides headless convex Voronoi fragments. `Fx/World/ShatterSpread.h` adds
timed visual crack propagation without putting effect state into geometry.

## Instrumentation

`Game::RenderStatistics()` returns draw calls, batches, vertices, uploads, state changes, glyph rasterizations
and surface allocations. `FrameObserver` measures runtime stages. `Game::ConfigureFrameExecution` temporarily
changes FPS/VSync without saving player options. See [Profiler](../Editor/Profiler.md).
