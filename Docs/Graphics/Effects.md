# Effects

Fx is headless and draws with [Painter](Painter.md): positions are `Float2`, colours `Color4`, cameras `CameraView`.
Every component takes the `Painter&` it draws into; nothing opens raylib state. `Fx/` and `Graphics/Draw/` never pick a
clock: effects advance with the delta their owner passes to `Update`, decorative cycles read `Painter::Time()` (the
presentation clock, frozen by the editor's Pause).

Most games need one `FxStage` and a few [cues](#cues). The parts below it are there for a view that wants only one of them.

## Graphics and FX

`ParticleSystem`, `FloatingTexts`, `Rings`, `Shockwaves`, `FlashLights`, `ScreenFlash`, `ScreenGlitch`, `Announcements`, `PulseOverlay`,
`SlowMotionOverlay` and `CameraEffects` can be used independently. They share one shape:

- `Update(dt)` with the chosen time domain, `Clear()` on scene/run reset, a const `Draw`. `Announcements` wants real time,
  and `SlowMotionOverlay` has both: `UpdateReal` for how long it shows, `Update` for its trails.
- An effect that happens at an event takes its style with the event, so two sizes of one effect can run together. A
  single continuous device, `CameraEffects` or `PulseOverlay`, takes its style at construction.
- An invalid style or time throws `std::invalid_argument` before changing state, in every configuration, and each style
  has an `IsValid`. Nonpositive or nonfinite update deltas do nothing. Zero-width optional decorations and zero fade/pop times disable those features.
- What the player chose comes through `SetQuality`, where it matters to that effect.

Styles hold plain data and are copied into active effects, so changing a preset affects only later emissions. Where an
effect has one obvious motion, its style names the `Curve` ([Animation](../Animation.md#curves)): `RingStyle::curve`,
`ShockwaveStyle::curve`, `FlashStyle::curve`, `FloatingTextStyle::popCurve`, `SlowMotionStyle::fadeCurve`.

```cpp
ParticleSystem particles({.capacity = 600, .seed = 1729});
ParticleEmission emission{.speed = {40.0f, 100.0f}, .lifetime = {0.4f, 0.8f},
                          .size = {2.0f, 5.0f}, .acceleration = {0.0f, 150.0f}, .drag = 1.0f};
ParticleAppearance appearance{.shape = ParticleShape::Square,
                              .colors = ParticleFadeOut(0.6f),
                              .size = {.start = 1.0f, .end = 0.5f}};
particles.Emit({300.0f, 200.0f}, Rgb(0xff9030), 24, emission, appearance);
// Copy and override a descriptor to create another simultaneously active variant.
emission.lifetime = {1.0f, 1.5f};
emission.area = {.shape = ParticleAreaShape::Box, .halfSize = {20.0f, 4.0f}};
particles.Emit({500.0f, 200.0f}, Rgb(0xff9030), 24, emission, appearance, [](Painter& paint, const ParticleDrawData& particle) {
    paint.Circle(particle.position, particle.size, {.stroke = {1.5f, particle.color}});
});
// In the View's Draw(Painter& paint):
particles.Draw(paint);
```

Capacity is per particle-system instance; when it is full, the oldest particles make room for new ones, and the player's particle setting
(`FxQuality::particlePercent`, given with `SetQuality`) scales every emission. The generator is the system's own `Rng`. Its
seed has a fixed default, so the same emissions look the same on every run: a frame comparison or a rewind sees the same
picture. Give another seed for two systems that should differ. Shapes are geometry choices, not stylistic recipes.

Through its life a particle follows its appearance: `colors` holds up to eight keys (`ParticleColorKey`, at 0..1 of its
life) joined by straight lines, and `size` goes from `start` to `end` times its emitted size along a `Curve`. The keys
multiply the colour given to `Emit`, so one look works in any colour: white to grey darkens it, alpha fades it, and
`ParticleFadeOut(fraction)` is the usual fade. With `ownColors` the keys' colours are used as they are, for a look with
colours of its own, like fire. No keys: the given colour, unfaded. `ParticleEmission::area` is where in the source they
start (a Point, Disc, Ring, Box or Line around it; `outward` aims each away from the centre), and `inherit` is the share
of the source's velocity each starts with: a `ParticleSource` is a position or a position and a velocity, and a plain
`Float2` converts to a still one. Particles with the same look share one entry of the system, so each holds only its own
position, motion, age and colour. A custom
draw, the last argument of `Emit`, replaces built-in drawing and receives resolved colour/size plus velocity, rotation
(radians, clockwise) and normalized age; it draws only (the blend is already set). Emission direction/spread are radians,
positions/sizes are canvas units, acceleration is units/second squared, spin is radians/second, clockwise. `drag` is
the share of velocity lost per second, whatever the frame rate. Alpha particles are drawn first, additive ones on top; both
stay in one draw call. Sprite particles capture a `TextureRef` or `Surface` and draw it with `paint.Image`; the View owns
the resource.

`CameraEffects::Apply(base, timeSeconds)` composes shake, kick, motion and punch with a `CameraView`, keeping its base
transform and rotation. `Shake(amount)` adds to a shake of at most 1, and what shows is its square: 0.3 is a light hit, 0.6 a
heavy one, and hits in a row build up. It offsets and turns the camera along smooth noise, and fades at `shakeDecay`. Punch focus is a world position; kick and motion directions use canvas axes. With no explicit
focus, punch uses the base target. Punch and motion settle at the same speed at any frame rate; the kick is a
`SpringPoint`, which clamps long frames. Draw through the result: `auto view = paint.Transform(camera.Transform())`.
`MotionIntensity()` feeds the same smoothed intensity to independently drawn speed lines: `DrawSpeedLines(paint, bounds,
intensity, color, time, style)` in `Graphics/Draw/SpeedLines.h` clips itself to `bounds`.

#### Choosing a text effect

| Need | Use |
| --- | --- |
| A short themed label, nothing else | `FxStage::FloatText` / `PopText` (no extra target) |
| A number: merging, formatting, intensity, particles | [NumberPopup](../../Core/Modules/NumberPopup/README.md) `Emit` |
| A text label with the same animation and juice | NumberPopup `EmitLabel` |
| Arbitrary drawing, or text with handles and no juice | [Popup](../../Core/Modules/Popup/README.md), with `PopupText` for text |

Popup and NumberPopup are explicitly owned/updated modules; the caller controls their placement relative to the FxStage
layers. They read no theme, so the owner copies any theme values it wants into their styles.

Floating text uses `FloatingTextStyle` for velocity, lifetime, pop/fade and `FxTextStyle` for size, the anchor point on
the text, width fitting and shadow (`Fx/FxText.h`; drawn with the game's font). `ScreenFlash` is a colour that fades over
the given bounds; a new flash replaces the one showing. `Announcements` takes its waiting-toast capacity at construction and
draws banner, then toast within caller-supplied bounds. New toasts shorten the current toast to its queued duration; a full
waiting queue drops its oldest entry. Dequeued toasts use queued duration while more entries wait, otherwise full duration.
Capacity zero disables waiting.

### Effect stage and particle recipes

`Core/Fx` keeps what ties effects together at its top: `FxStage`, cues, `FxStyle`, `FxQuality` and `FxText`. Each
effect sits in a folder named for what it acts on:

| Folder | Effects |
| --- | --- |
| `Fx/Particles` | `ParticleSystem`, `ParticleRecipes` |
| `Fx/Camera` | `CameraEffects`, `CameraShot` |
| `Fx/World` | `Rings`, `Shockwaves`, `FloatingTexts`, `FlashLights`, `Marks`: at a point in the field |
| `Fx/Screen` | `ScreenFlash`, `FlashLimit`, `ScreenGlitch`, `PulseOverlay`, `SlowMotionOverlay`, `Announcements`: over the view |

`Fx/FxStage` bundles every component above for one play area. In a scene it usually comes with a
[Stage](Stage.md) (`Stage::Fx()`), which updates it on its two clocks, follows the player's choice, gives it the time scale
and draws its layers in their places for every view; the rest of this section is what that does underneath. Construct it with the play area, the view area for edge
effects and an `FxStyle` (`Fx/FxStyle.h`), normally `CurrentTheme().fx`. Positions are canvas coordinates; the game adapts
its own grid. An invalid style throws here, naming the part, not when that effect first plays.

Two calls a frame, one per clock, from the scene's two hooks: `UpdateReal(realSeconds)` from `Scene::UpdateReal`, which runs
in a hitstop and a pause too, and `Update(gameSeconds)` from `Scene::Update`. Effects in the world, cue delays and cooldowns
run on game time, so a hitstop holds them. Banners and toasts run on real time, since reading does not slow down with the
game, and so does how long slow motion's look lasts, so it ends when `TimeScale` does. Each call moves only its own clock's
parts, so a scene in `TimeDomain::Real`, whose hooks both get real seconds, moves nothing twice.

A game whose camera follows or scrolls passes its view as `fx.WorldCamera(paint, camera.View(), camera.Settings().limits)`
([Camera](../Camera.md#effects-over-a-game-camera)). Effect positions are then world positions; a shot blends from that view
by its weight and stays at the view's place on the canvas, and with `keepInArena` it stays inside the limits.

```cpp
FxStage fx(playArea, viewArea, CurrentTheme().fx);
fx.SetQuality(Effects());
fx.Play("explosion", {.at = at, .color = orange}); // A cue
fx.PopText(at, "x3", yellow, 40.0f);               // One effect
// Drawing:
{
    auto clip = paint.Clip(viewArea);
    auto camera = paint.Transform(fx.WorldCamera(paint).Transform());
    DrawFloor(paint);
    fx.DrawGroundLayer(paint); // Inside the world camera, under the objects: marks
    DrawWorld(paint);
    fx.DrawWorldLayer(paint); // Inside the world camera, over the objects
}
fx.DrawOverlay(paint); // No camera: vignette, speed lines, flash / banner / toast, glitch
```

`Mark(at, color, style)` (the cue action `FxMark`) leaves a mark on the ground: a scorch, a splash, the dust of a landing,
or with `growSeconds` the shadow of something about to fall. It lies under everything that stands, fades over
`fadeSeconds` and runs on game time; the oldest gives way past `Marks::MaxLying`.

`SetQuality` is one call for the whole stage, and every owner without a `Stage` has to make it. With flashes off there are also no
shockwaves, no banner curtain and no [light flashes](Lighting.md#short-flashes). Flashes and light flashes share one limit of
three starts a second whatever the setting (`FlashLimit`), and a new flash takes over only if it is as bright as the one showing. `FxStage::SlowMotionLook` is only how slow
motion looks, and `TimeScale::SlowMo` only slows time; `FxSlowMotion` in a cue does both.

`Fx/Particles/ParticleRecipes` holds what the bursts are. `ParticleRecipe` names six (Soft, Spark, Sparkle, Debris,
Pixel, Confetti), and `ParticleRecipes`, in `Theme::fx.particles`, says what each looks like, plus dash and steam layers
and four streams (below). A burst is plain data, two `ParticleLayer`s, so a game's own needs no change here:
`fx.Burst(at, color, 24, myBurst)`, with an optional custom draw for sprites. `EmitBurst` spreads a count over a burst's
layers; views that own a bare `ParticleSystem` use it with `CurrentTheme().fx.particles.Burst(recipe)`. `Fx` never reads
the theme. `FxPlay::velocity` is what a cue's particles inherit.

#### Streams

Something that keeps going (the trail behind a moving object, smoke from a chimney, a torch) owns a `ParticleEmitter`
and calls it each frame; it is a value like a tween, so it goes when its object goes:

```cpp
ParticleEmitter _trail{CurrentTheme().fx.particles.trail}; // A member of the moving object
fx.Emit(_trail, position, color, gameSeconds);              // Each frame
_trail.Jump();                                              // After a teleport or a pause: do not fill the way
```

`ParticleEmitterStyle` makes `perSecond` particles each second and `perUnit` for each canvas unit the source travels,
carrying fractions over, so the count does not depend on the frame rate. Each frame's particles are spread along the way
since the last call and aged by the rest of the frame, so a fast source leaves an even line; that way is also the
source's velocity for `inherit`. The player's particle setting scales the rate, so it thins even one particle a frame.
The theme has four: `trail`, `smoke` (takes the colour it is given), and `flame` and `embers` (their own colours). Rain,
snow and dust that fill the view are [Atmosphere](Atmosphere.md)'s `DriftField`, not emitters.

Neon widgets and their styles are described in [Painter → Helpers and theme](Painter.md#helpers-and-theme).

### Cues

A cue is what one event looks like, as constant data; a stage plays it by name. Each action is one stage call. Its place,
colour, direction and text come from `FxPlay`, and its look from the stage's style, so one cue works with any theme.

```cpp
fx.SetCues(DefaultFxCues().With({
    {"player_hit", {FxBurst{ParticleRecipe::Debris, 24}, FxShake{0.5f}, FxHitstop{0.05f},
                    {FxFlash{}, White},           // Its own colour
                    {0.08f, FxShockwave{120.0f}}}, // Later, on game time
     0.1f},                                       // Cooldown: at most one every 0.1 seconds
}));
fx.SetTime(&Time());                              // Only for FxHitstop and FxSlowMotion
fx.Play("player_hit", {.at = at, .color = red, .intensity = damage / maxDamage});
```

Bad values throw when the cue is made. An unknown name throws where it plays, and a cue that changes time throws without
`SetTime`, before any of it plays. A stage starts with `DefaultFxCues()`: `hit`, `explosion`, `pickup`. Effects with
parameters of their own stay methods (`fx.Shake(0.3f)`), so the compiler checks them: names are for cues only.

`FxPlay::intensity` sizes one play of a cue: particle counts, shake, punch, kick, ring and reach sizes, a flash's opacity and a light flash's strength.
Times, texts and colours stay. A cooldown, the third value of a set's entry, keeps a name from playing again for that many
seconds of game time: `Play` returns false, and fifty hits in a frame look like one instead of fifty flashes.

A clip's moment with the same name as a cue connects animation to effects, with no pointer or stored callback in either:

```cpp
animator.Update(seconds, [&](std::string_view moment) {
    if (fx.Cues().Contains(moment)) fx.Play(moment, {.at = hand});
});
```

A simulation does not call Fx ([Simulation](../Simulation.md)). It keeps a count or an event list, and the view plays the cue;
see `AgentDocs/Recipes/FxCues.md`. In the editor a seek runs the same frames, so cues play and age as they did in play.

## Cinematic camera and configurable overlays

`Fx/Camera/CameraShot.h` defines `CameraShot`, `FrameCameraShot` and `ShotCamera`. A shot is an animation pose: `Mix`
blends two, `SecondsTween<CameraShot>` moves between them, and a `SecondsClip<CameraShot>` in an animator is an authored camera move
with named moments. Hand the result to `FxStage::SetShot` each frame.

`CameraFraming` takes explicit viewport, safe area, padding, and positive ordered zoom limits. Empty bounds produce a neutral
shot. The safe area is intersected with the viewport before padding. `ShotCamera(shot, viewport, arena)` blends the shot with
the neutral camera and optionally keeps the zoomed viewport within the arena; it returns a `CameraView`. A shot's rotation is
in radians, clockwise. Supply geometry in the same coordinate space. Apply `CameraEffects::Apply` afterward to retain
cinematic positioning beneath shake, kick and punch. Letterboxes, tile/grid conversion and game layout stay in the caller's
adapter.

`ScreenGlitch`, `Shockwaves` and `PulseOverlay` are independent value-owned components. Their styles configure colour,
intensity, timing, sizes and counts. Use `Trigger(seconds, style)`, `Emit(center, color, style, reach)` or `SetActive(bool,
color)`, respectively, then `Update(dt)` and `Clear()` on reset. Glitch retriggering retains the greater of the requested
duration and remaining time; shockwaves overlap as separate emissions. Pulse phase advances while active or fading out.
Glitch drawing takes explicit bounds (it clips to them) and the time picking its pattern; shockwaves derive a default reach
from the supplied bounds. Pulse offers separate `DrawTint(paint, bounds)` and `DrawVignette(paint, bounds)` calls so the owner
controls layering. The theme (`FxStyle`) supplies palette and tuning; Engine reads no game constants.

`Graphics/Draw/IrisTransition.h` provides stateless `DrawIrisTransition(paint, bounds, focus, cover, style)`. Coverage is
clamped to `[0, 1]`; zero is open and one is covered. `IrisStyle` supplies curtain, inner-rim, outer-rim and glow colours plus
rim/glow widths in canvas units. Glow alpha sets its additive strength, and nonpositive widths disable their rings. The
helper clips to the supplied rectangle; empty bounds draw nothing. Timing is `Scene/TransitionClock.h`: `Cover()` goes to it,
and `Update` tells when the curtain is closed. Focus changes and destinations remain in the consuming game.

`Utilities/Geometry/ShatterPattern.h` builds the convex Voronoi shards and shared crack edges. Pass that immutable geometry
to `Fx/World/ShatterSpread.h` to calculate when each crack opens and each shard becomes free. With no sources the pattern is
open at time zero; speed must always be positive.

## General usage rules

The View keeps its effects as value members; it calls Update with the chosen delta and Clear on reset. The game decides
which effect accompanies each event and how world coordinates map to the canvas. Draw callbacks only draw; callbacks
capturing textures do not own the resource. Decorative cycles read `paint.Time()` (editor Pause freezes it).
