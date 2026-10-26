# Effects

## Graphics and FX

`ParticleSystem2D`, `FloatingText2D`, `RingEffects2D`, `CameraEffects2D` and `OverlayEffects2D` can be used
independently. Call `Update(dt)` with the chosen time domain and `Clear()` on scene/run reset; `Draw` is const.
Emission descriptors are copied into active effects, so changing a preset affects only subsequent emissions.
Invalid required durations/ranges throw `std::invalid_argument` before changing state. Nonpositive/nonfinite
update deltas do nothing. Zero-width optional decorations and zero fade/pop times disable those features.

```cpp
ParticleSystem2D particles({.capacity = 600, .seed = 1729});
ParticleEmission emission{.speed = {40.0f, 100.0f}, .lifetime = {0.4f, 0.8f},
                          .size = {2.0f, 5.0f}, .acceleration = {0.0f, 150.0f}, .drag = 1.0f};
ParticleAppearance appearance{.color = ORANGE, .shape = ParticleShape::Square,
                              .endSizeScale = 0.5f};
particles.Emit({300.0f, 200.0f}, 24, emission, appearance);
// Copy and override a descriptor to create another simultaneously active variant.
emission.lifetime = {1.0f, 1.5f};
appearance.draw = [](const ParticleDrawData& particle) {
    DrawPoly(particle.position, 6, particle.size, particle.rotation, particle.color);
};
particles.Emit({500.0f, 200.0f}, 24, emission, appearance);
```

Capacity is per particle-system instance; excess new particles are dropped. An omitted seed uses
`std::random_device`. Shapes are geometry choices, not stylistic recipes. A callback replaces built-in
drawing and receives resolved color/size plus velocity, rotation (degrees), and normalized age. Emission
direction/spread are radians, positions/sizes are canvas units, acceleration is pixels/second squared,
and spin is degrees/second. The callback must only draw: do not mutate the system or change camera,
scissor, render-target or blend state. Particle drawing performs alpha then additive passes and returns
to alpha blending. Call it with alpha blending active.

For sprite particles, a callback can capture a `Texture2D` handle and call `DrawTexturePro`. The View
owns/unloads that texture; declare resource owners before dependent FX members, clear dependent effects
before unloading, and keep the View inside the renderer/window lifetime. Callbacks own their closures,
not resources referenced by those closures. No engine texture catalog or asset manager is required.

`CameraEffects2D::Apply(baseCamera, timeSeconds)` composes shake, kick, motion and punch with an existing
camera, retaining its base transform and rotation. Punch focus is a world position. Kick and motion
directions use canvas axes; their offsets are canvas pixels. With no explicit focus, punch uses the base
target. `MotionIntensity()` lets the View feed the same smoothed intensity to independently drawn speed
lines. `DrawSpeedLines(bounds, intensity, color, timeSeconds, style)` owns its scissor scope; call with no
existing scissor. `Fx/`, `Graphics/Draw/` and `Graphics/Render/` never choose a clock; they take time as a parameter.
The presentation clock `DrawTime()` (`Graphics/Draw/DrawTime.h`) is written by `Game` before each frame from the
virtual `Game::DrawSeconds()` (real time by default; a preview host returns its pausable clock). Game views and the
themed `Graphics/Ui/` layer (below) read it for decorative cycles; nothing calls `GetTime()` for drawing.

Floating text uses `FloatingTextStyle` for velocity, lifetime, pop/fade and `TextStyle` for nominal font
size, normalized anchor, width fitting and shadow. `TextRenderer::DrawAnchored` shares that rendering
with overlay text. `OverlayEffects2D` takes its waiting-toast capacity at construction and explicit
styles per trigger. It draws flash, banner, then toast within caller-supplied bounds. New toasts shorten
the current toast to its queued duration; a full waiting queue drops its oldest entry. Dequeued toasts
use queued duration while more entries wait, otherwise full duration. Capacity zero disables waiting.

### Effect stage and particle recipes

`Fx/FxStage` bundles every component above for one play area: particles, floating/pop text, rings, shockwaves,
camera (shake, punch, kick, motion lead, cinematic `CameraShot`), flash/banner/toast, pulse, glitch and slow motion.
Construct it with the play area, the view area for edge effects and an `FxStyle` (`Fx/FxStyle.h`), normally
`CurrentTheme().fx`. Positions are canvas coordinates; the game adapts its own grid. `WorldCamera(time)` and
`DrawOverlay(text, time)` take the decorative clock explicitly; `DrawWorldLayer` goes inside that camera.
`Fx/ParticleRecipes` holds ready-made bursts (`ParticleRecipe`: Soft, Spark, Sparkle, Debris, Pixel, Confetti)
plus trail, dash and steam layers, all scaled by a `ParticleRecipeTuning`. `EmitBurst` spreads a count over a
recipe's layers; Views that own a bare `ParticleSystem2D` use it with `CurrentTheme().fx.particles`. `Fx` never
reads the theme.

```cpp
FxStage fx(playArea, viewArea, CurrentTheme().fx);
fx.Burst(at, ORANGE, 24, ParticleRecipe::Debris);
fx.PopText(at, "x3", YELLOW, 40);
fx.Shake(0.3f);
```

`Graphics/Draw/Neon.h` takes explicit frame/text/panel/chip/arcade/round styles. `ButtonLook` combines
`ButtonAnimState` with accent, entrance progress and offset. The headless `ButtonAnim` takes spring and
feedback settings; its defaults select immediately without press/deny animation. `Clear()` retains
settings. Animated arcade drawing receives `timeSeconds` explicitly. These helpers expect alpha
blending and return to it; the caller controls clipping. Optional glow/core widths can be disabled.


## Cinematic camera and configurable overlays

`Fx/CameraShot.h` defines `CameraShot`, `BlendShots`, `FrameCameraShot` and `ShotCamera` in the runtime Engine.
`CameraFraming` takes explicit viewport, safe area, padding, and positive ordered zoom limits. Empty bounds
produce a neutral shot. The safe area is intersected with the viewport before padding. `ShotCamera(shot, viewport,
arena)` blends the shot with the neutral camera and optionally keeps the zoomed viewport within the arena.
Supply geometry in the same coordinate space. Apply `CameraEffects2D::Apply` afterward to retain cinematic positioning
beneath shake, kick and punch. Letterboxes, tile/grid conversion and game layout stay in the caller's adapter.

`GlitchEffects2D(GlitchStyle)`, `ShockwaveEffects2D(ShockwaveStyle)` and `PulseOverlay2D(PulseOverlayStyle)` are independent
value-owned runtime components. Their styles configure color, intensity, timing, sizes and counts. Supply valid
positive periods/durations and nonnegative dimensions/counts. Use `Trigger(seconds)`, `Emit(center, color, reach)` or
`SetActive(bool, Color)`, respectively, then `Update(dt)` and `Clear()` on reset. Glitch retriggering retains the greater of
the requested duration and remaining time; shockwaves overlap as separate emissions. Pulse phase advances while active
or fading out. Glitch drawing takes explicit `Rect` bounds and `timeSeconds`; shockwaves derive a default reach from
the supplied bounds. Pulse offers separate `DrawTint(bounds)` and `DrawVignette(bounds)` calls so the owner controls
layering. These draws restore additive blending to alpha; glitch owns a scissor scope, so call it outside any enclosing
scissor scope. The theme (`FxStyle`) supplies palette and tuning; Engine reads no Game constants.

`Fx/IrisTransition.h` provides stateless `DrawIrisTransition(bounds, focus, cover, style)` in the
raylib runtime. Coverage is clamped to `[0, 1]`; zero is open and one is covered. `IrisStyle` supplies
curtain, inner-rim, outer-rim, and glow colors plus rim/glow widths in canvas pixels. Glow alpha
sets its additive strength, and nonpositive widths disable their rings. The helper clips to the
supplied rectangle, including offset bounds; empty bounds draw nothing. Call it during overlay
drawing with no other scissor scope active. Transition timing, focus changes, and destinations
remain in the consuming game.

## Genel kullanım kuralları

View kendi efektlerini değer üye olarak tutar; seçilen delta ile Update, reset sırasında Clear çağırır.
Oyun hangi olaya hangi efektin verileceğini ve dünya koordinatlarını canvas'a nasıl çevireceğini belirler.
Draw callback'leri çizim state'ini korur; texture yakalayan callback kaynağın sahibi değildir.
Önizleme Pause/Step davranışı için dekoratif çizimde GetTime yerine DrawTime kullanın.
