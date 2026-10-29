# Rendering

`GameSettings::canvasWidth/canvasHeight` mantıksal çalışma alanıdır (varsayılan 1280×720).
Pencere daha büyükse canvas texture pencerenin piksel boyutunda rasterize edilir; mantıksal
koordinatlar projeksiyonla korunur. Küçük pencere mantıksal boyuttan küçültülür.
En-boy oranı korunur, dış alan `letterbox` rengiyle doldurulur.
Piksel tabanlı raylib clipping yerine `BeginClip` veya UiRegion içinde `BeginUiClip` kullanın.

`Game` renderer frame'ini açar; normal sahne/View kodu `BeginDrawing` veya
`BeginTextureMode` ile ikinci bir ana frame açmaz. Pencere, canvas ve renderer Engine-private'tır
(`Graphics/Render/Internals/`); Engine içinde onları kullanan kod RAII `Renderer::Frame` sözleşmesine uyar.
`CanvasReady` tamamlanmış canvas'ı ödünç verir; texture sahipliği devredilmez.
`DrawWindow` canvas sunumundan sonra pencere koordinatında çizim içindir.

`Graphics/Render/RenderLayer` is an offscreen target (RAII render texture, constructed after the window) whose image
is later drawn as a texture. `Begin`/`End` nest inside the canvas or another layer; `End` rebinds the previous
target with its projection. Open and close cameras and scissor scopes inside the layer. `MakeOpaque()` (no camera)
sets the layer's alpha to one without touching colors, so translucent draws do not leave see-through patches.
The texture is vertically flipped: layer point `(x, y)` is UV `(x / width, 1 - y / height)`.
`DrawUtils` adds `FillPolygon` and `DrawTexturedPolygon` for convex polygons in either winding, so mirrored
(tumbling) pieces still draw with back-face culling enabled; `FillTriangle` and `FillQuad` (split along a-c) also
accept either winding.

`Graphics/Draw/GlowBatch.h` draws consecutive radial glows in one additive blend scope. Create a local
`GlowBatch` with alpha blending active and call `Draw(center, radius, color)` for each glow. It keeps the
same gradient geometry and submission order as `DrawGlow`, without a queue or per-glow blend changes.
The first enabled glow opens additive blending; destruction returns to alpha. An empty batch, or one
with glow quality Off, does not change blend state. End the batch scope before any other drawing or
blend, camera, scissor or render-target change; batches must not overlap. Keep standalone `DrawGlow`
where alpha drawings are interleaved, so their composition order is preserved.

`RenderExecutionOptions` is a transient Renderer-owned frame policy. Its default Batched glow mode uses
the scope above; Individual routes each batch submission through DrawGlow for controlled A/B comparisons.
The scoped execution context follows nested render layers and optionally reports glow/blend requests to
the borrowed FrameObserver. These counters are helper requests, not total GPU draw calls. Player graphics
preferences are separate. Development hosts use Game::ConfigureFrameExecution to save/restore this policy,
the FPS limit and VSync without writing preferences. [Profiler](../Editor/Profiler.md) uses this contract.

Shared view math is header-only and headless, so games need no local copies:

| Header | Helpers |
| --- | --- |
| `Graphics/Anim/Easing.h` | curves (`EaseInCubic`, `EaseOutCubic`, `EaseInOutCubic`, `EaseOutBack`; all clamp `t`), timeline helpers `Progress01(t, from, to)`, `Since(t, moment)`, `EaseOutPulse(t, at, seconds)`, `Staggered(t, delay, index, step, seconds)`, `Crossed(before, after, moment)`, plus `CountDown`, `BlinkOff` |
| `Utilities/MathUtils.h` | `Mix`, `Fract`, integer `Hash`/`Random01`, sin-based `NoiseHash(a, b)` (distributes differently from `Hash`), `WrapIndex(value, count)`, `ClampIndex(value, count)` |

The headless `Fx/ShatterPattern` splits a `Rect` into convex Voronoi shards around seed points (shard index =
seed index; outside or repeated seeds yield empty shards) and lists the cracks between neighbouring shards,
excluding the rectangle's border. `Spread(sources, speed)` times a wave from `(point, time)` sources: each crack
opens from the end the wave reaches first and runs to the other end at the same speed; each shard records when
its first crack starts and when all its cracks have opened. `ShardAt` returns the shard containing a point.


## UI coordinate transforms

`Utilities/UiTransform.h` provides headless centered, aspect-preserving mapping between positive-size
source and target `Rect`s. Invalid dimensions throw `std::invalid_argument`. `Map` / `Unmap` accept
explicit `Float2` or integer `Vec2` points; `Scale` and `Offset` expose the camera mapping. Rectangle
centers use `Rect::CenterX/Y` (integer division even for odd sizes). Integer results use `std::lround`,
with integer forward mapping centered before rounding to retain layout targets at half-pixel ties.

```cpp
const UiTransform transform(designArea, panelArea);
const Vec2 animationTarget = transform.Map(Vec2{designX, designY});
const Float2 localPoint = transform.Unmap(Float2{canvasX, canvasY});
```

`Graphics/Render/UiRegion.h` adds raylib `Vector2` mapping and `Draw(callable)`, using the same transform.
It does not choose widget areas, clip automatically, or convert window coordinates. Supply all areas
in logical canvas coordinates. Call `Draw` inside the renderer frame with no other 2D camera active;
callbacks must not change camera/render-target state. The camera is ended even if the callback throws.

`BeginUiClip(Rectangle)` applies the current modelview's positive axis-aligned scale/translation to
design coordinates, preserving floor-origin/ceil-size scissor rounding. End with `EndScissorMode`.
It replaces the current scissor, so callers must reapply an outer clip themselves. Rotation, negative
scale, automatic clip intersection, and nested camera/scissor restoration are not supported.


Kamera, layer, scissor ve blend kapsamlarını açıldıkları sıra tersinde kapatın.
Scissor değiştiren yardımcıların otomatik dış clip restorasyonu yaptığını varsaymayın.
