# Effects

Fx is headless and draws with [Painter](Painter.md): positions are `Float2`, colours `Color4`, cameras `CameraView`.
Every component takes the `Painter&` it draws into; nothing opens raylib state. `Fx/` and `Graphics/Draw/` never pick a
clock: simulations advance with the delta their owner passes to `Update`, decorative cycles read `Painter::Time()` (the
presentation clock, frozen by the editor's Pause).

## Graphics and FX

`ParticleSystem2D`, `FloatingText2D`, `RingEffects2D`, `CameraEffects2D` and `OverlayEffects2D` can be used
independently. Call `Update(dt)` with the chosen time domain and `Clear()` on scene/run reset; `Draw(paint)` is const.
Emission descriptors are copied into active effects, so changing a preset affects only subsequent emissions. Invalid
required durations/ranges throw `std::invalid_argument` before changing state. Nonpositive/nonfinite update deltas do
nothing. Zero-width optional decorations and zero fade/pop times disable those features.

```cpp
ParticleSystem2D particles({.capacity = 600, .seed = 1729});
ParticleEmission emission{.speed = {40.0f, 100.0f}, .lifetime = {0.4f, 0.8f},
                          .size = {2.0f, 5.0f}, .acceleration = {0.0f, 150.0f}, .drag = 1.0f};
ParticleAppearance appearance{.color = Rgb(0xff9030), .shape = ParticleShape::Square, .endSizeScale = 0.5f};
particles.Emit({300.0f, 200.0f}, 24, emission, appearance);
// Copy and override a descriptor to create another simultaneously active variant.
emission.lifetime = {1.0f, 1.5f};
appearance.draw = [](Painter& paint, const ParticleDrawData& particle) {
    paint.Circle(particle.position, particle.size, {.stroke = {1.5f, particle.color}});
};
particles.Emit({500.0f, 200.0f}, 24, emission, appearance);
// In the View's Draw(Painter& paint):
particles.Draw(paint);
```

Capacity is per particle-system instance; excess new particles are dropped, and the player's particle setting
(`FxQuality::particlePercent`, given with `SetQuality`) scales every emission. An omitted seed uses `std::random_device`. Shapes are geometry
choices, not stylistic recipes. A callback replaces built-in drawing and receives resolved colour/size plus velocity,
rotation (radians, clockwise) and normalized age; it draws only (the blend is already set). Emission direction/spread
are radians, positions/sizes are canvas units, acceleration is units/second squared, spin is radians/second, clockwise.
Alpha particles are drawn first, additive ones on top; both stay in one draw call. Sprite particles capture a
`TextureRef` or `Surface` and draw it with `paint.Image`; the View owns the resource.

`CameraEffects2D::Apply(base, timeSeconds)` composes shake, kick, motion and punch with a `CameraView`, keeping its base
transform and rotation. Punch focus is a world position; kick and motion directions use canvas axes. With no explicit
focus, punch uses the base target. Draw through the result: `auto view = paint.Transform(camera.Transform())`.
`MotionIntensity()` feeds the same smoothed intensity to independently drawn speed lines: `DrawSpeedLines(paint, bounds,
intensity, color, time, style)` clips itself to `bounds`.

Floating text uses `FloatingTextStyle` for velocity, lifetime, pop/fade and `FxTextStyle` for size, the anchor point on
the text, width fitting and shadow (`Fx/FxText.h`; drawn with the game's font). `OverlayEffects2D` takes its
waiting-toast capacity at construction and explicit styles per trigger. It draws flash, banner, then toast within
caller-supplied bounds. New toasts shorten the current toast to its queued duration; a full waiting queue drops its
oldest entry. Dequeued toasts use queued duration while more entries wait, otherwise full duration. Capacity zero
disables waiting. Flashes (and glitches) follow the player's `FxQuality::flashes` (`SetQuality`).

### Effect stage and particle recipes

`Fx/FxStage` bundles every component above for one play area: particles, floating/pop text, rings, shockwaves, camera
(shake, punch, kick, motion lead, cinematic `CameraShot`), flash/banner/toast, pulse, glitch and slow motion. Construct
it with the play area, the view area for edge effects and an `FxStyle` (`Fx/FxStyle.h`), normally `CurrentTheme().fx`.
Positions are canvas coordinates; the game adapts its own grid.

```cpp
FxStage fx(playArea, viewArea, CurrentTheme().fx);
fx.Burst(at, orange, 24, ParticleRecipe::Debris);
fx.PopText(at, "x3", yellow, 40.0f);
fx.Shake(0.3f);
// Drawing:
{
    auto clip = paint.Clip(viewArea);
    auto camera = paint.Transform(fx.WorldCamera(time).Transform());
    DrawWorld(paint);
    fx.DrawWorldLayer(paint); // Inside the world camera, over the objects
}
fx.DrawOverlay(paint); // No camera: vignette, speed lines, flash / banner / toast, glitch
```

`Fx/ParticleRecipes` holds ready-made bursts (`ParticleRecipe`: Soft, Spark, Sparkle, Debris, Pixel, Confetti) plus
trail, dash and steam layers, all scaled by a `ParticleRecipeTuning`. `EmitBurst` spreads a count over a recipe's
layers; Views that own a bare `ParticleSystem2D` use it with `CurrentTheme().fx.particles`. `Fx` never reads the theme.

Neon widgets and their styles are described in [Painter → Helpers and theme](Painter.md#helpers-and-theme).

## Cinematic camera and configurable overlays

`Fx/CameraShot.h` defines `CameraShot`, `BlendShots`, `FrameCameraShot` and `ShotCamera`. `CameraFraming` takes explicit
viewport, safe area, padding, and positive ordered zoom limits. Empty bounds produce a neutral shot. The safe area is
intersected with the viewport before padding. `ShotCamera(shot, viewport, arena)` blends the shot with the neutral
camera and optionally keeps the zoomed viewport within the arena; it returns a `CameraView`. A shot's rotation is in
radians, clockwise. Supply geometry in the same coordinate space. Apply `CameraEffects2D::Apply` afterward to retain
cinematic positioning beneath shake, kick and punch. Letterboxes, tile/grid conversion and game layout stay in the
caller's adapter.

`GlitchEffects2D(GlitchStyle)`, `ShockwaveEffects2D(ShockwaveStyle)` and `PulseOverlay2D(PulseOverlayStyle)` are
independent value-owned components. Their styles configure colour, intensity, timing, sizes and counts. Supply valid
positive periods/durations and nonnegative dimensions/counts. Use `Trigger(seconds)`, `Emit(center, color, reach)` or
`SetActive(bool, color)`, respectively, then `Update(dt)` and `Clear()` on reset. Glitch retriggering retains the
greater of the requested duration and remaining time; shockwaves overlap as separate emissions. Pulse phase advances
while active or fading out. Glitch drawing takes explicit bounds (it clips to them) and the time picking its pattern;
shockwaves derive a default reach from the supplied bounds. Pulse offers separate `DrawTint(paint, bounds)` and
`DrawVignette(paint, bounds)` calls so the owner controls layering. The theme (`FxStyle`) supplies palette and tuning;
Engine reads no game constants.

`Fx/IrisTransition.h` provides stateless `DrawIrisTransition(paint, bounds, focus, cover, style)`. Coverage is clamped
to `[0, 1]`; zero is open and one is covered. `IrisStyle` supplies curtain, inner-rim, outer-rim and glow colours plus
rim/glow widths in canvas units. Glow alpha sets its additive strength, and nonpositive widths disable their rings. The
helper clips to the supplied rectangle; empty bounds draw nothing. Transition timing, focus changes and destinations
remain in the consuming game.

## General usage rules

The View keeps its effects as value members; it calls Update with the chosen delta and Clear on reset. The game decides
which effect accompanies each event and how world coordinates map to the canvas. Draw callbacks only draw; callbacks
capturing textures do not own the resource. Decorative cycles read `paint.Time()` (editor Pause freezes it).
