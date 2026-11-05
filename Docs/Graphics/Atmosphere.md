# Atmosphere

Rain, snow, dust and fog that fill the view and stay filled, and the plain value that says which of them it is now. Design
record: `Docs/Design/README.md` → D42 (engine repository). Worked example:
[AgentDocs/Recipes/Weather.md](../../AgentDocs/Recipes/Weather.md).

| Header | Use |
| --- | --- |
| `Weather/Weather.h` | `Weather`: what the weather is now, as plain values; draws nothing, so a simulation may keep it |
| `Graphics/Atmosphere/Atmosphere.h` | `Atmosphere`: shows a `Weather`, behind and in front of what stands in the world |
| `Graphics/Atmosphere/DriftField.h` | `DriftField`: one field of drifting things; a game's own ash or leaves |
| `Graphics/Atmosphere/FogField.h` | `FogField`: one sheet of fog |
| `Graphics/Atmosphere/GroundRipples.h` | `GroundRipples`: rings on a floor seen from above |
| `Fx/World/Bolts.h`, `Fx/FxCue.h` | `FxBolt` and the default `lightning` cue |

## Two questions, two places

`Weather` answers "what is the weather now". `Atmosphere` answers "what does that look like". The first is data, and the
second draws, in the same way that a `Camera2D` makes a `CameraView` and `paint.Camera` uses it.

```cpp
SecondsTween<Weather> _weather{Weather::Clear()}; // Members
Atmosphere _air{TopDownAtmosphere()};
LightLayer _lights;
```
```cpp
_weather.To(Weather::Storm(), 8.0f, Curve::InOutSine); // Eight seconds from whatever it is now
```
```cpp
_weather.Update(seconds);
_air.Update(seconds, _weather);
```

There is no weather system to set up: a change of weather is a tween, a sequence is a clip, and a forecast is whatever code the
game wants. If a rule depends on the weather (wet ground slips, wind pushes arrows), keep the `Weather` in the simulation's
state and let the view read it. Its fields are listed for `State`, so it reaches the replay hash. Rules never read an
`Atmosphere`: it is view code on a view's clock.

## How a frame goes

```cpp
{ auto world = paint.Camera(view); DrawFloor(paint); }
_air.DrawBehind(paint, view);
{ auto world = paint.Camera(view); DrawCharacters(paint); }
_air.DrawFront(paint, view);
_lights.Draw(paint, frame, view);
{ auto world = paint.Camera(view); DrawNeon(paint); }
DrawHud(paint);
```

- **Order is the tool.** Fog from `DrawBehind` passes under a character's feet; rain from `DrawFront` falls across its face.
- **Before a `LightLayer`, the air is lit like the world.** In the dark nothing shows, under a lamp fog and dust appear, and
  lightning shows them all at once. Drawn after the layer they glow by themselves, which is what fireflies want.
- **Call both outside `paint.Camera`**, with the view the world was drawn through, shake included. Inside a camera it is an
  error, as with a `LightLayer`. An empty view suits a game without a camera. A split screen calls them for each view, with one
  `Atmosphere`.
- **The clock is yours.** Pass the game's delta and a hitstop holds the rain; pass a real one and it keeps falling. Not calling
  `Update` freezes it, as a pause menu wants.

## Seen from above, seen from the side

| | `TopDownAtmosphere()` | `SideViewAtmosphere(groundY)` |
| --- | --- | --- |
| Rain | Short streaks | Long streaks |
| Floor | Ripples anywhere | None |
| Fog | Patches anywhere | A band along the ground, at `groundY` |

Both are plain data: change what the game wants and pass it to the constructor. A density of 0 or a clear colour leaves a part
out. A rain that lands on a side view's ground with a splash is not there yet, because the air does not know where the ground is.

## The camera

Each layer has a `parallax`: `1` stands in the world, `0` stays on the screen, and above `1` moves faster than the world, as
what is close to the eye does. Rain that is stuck to the screen looks wrong when the camera moves, since the world slides
under it, so the defaults stand in the world or close to it. `CameraView::Parallaxed` gives the same view to your own drawing.

## A kind of your own

Rain, snow and dust are one class with different data, so anything that drifts needs no change in the engine:

```cpp
DriftField _ash{{.density = 0.6f,
                 .fall = {0.0f, 24.0f},
                 .size = {1.5f, 3.0f},
                 .spin = {-2.0f, 2.0f},
                 .sway = 18.0f,
                 .shape = ParticleShape::Square,
                 .color = {60, 56, 54, 220}}};
```
```cpp
_ash.Update(seconds, wind);
_ash.Draw(paint, amount, view);
```

`DriftLook::draw` draws a piece yourself, such as a leaf. `blend = Blend::Add` makes embers. The amount says how many of them
show, one at a time, so tween it freely. For a short burst at a point, use `ParticleSystem`.

## Lightning

```cpp
_fx.Play("lightning", {.at = strikePoint});
```

The default cue is a whole-scene light flash, a bolt and a small shake; write your own from `FxLightFlash`, `FxBolt`, `FxShake`
and a sound. Give `.flashes = _fx.LightFlashes()` to the light frame, and draw `_fx.DrawWorldLayer` after the layer, since the
bolt shines by itself. It obeys the player's `flashes` choice and the three-a-second limit. When to strike is the game's: a
timer in a view, or an event from a simulation if a strike changes what happens.

## Cost

A field keeps one tile of pieces, and a view larger than a tile thins out, so zooming far out costs no more than one tile's
worth. Fog computes noise at a coarse grid over the view each frame. `RenderQuality::backgroundEffects` off shows half the
pieces and doubles the size of fog's grid. Measurements: D42.
