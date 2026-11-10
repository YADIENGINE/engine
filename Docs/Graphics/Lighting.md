# Lighting

A dark world with lights and shadows in it. Design record: `Docs/Design/README.md` → D38 (engine repository). Worked example:
[AgentDocs/Recipes/Lighting.md](../../AgentDocs/Recipes/Lighting.md).

| Header | Use |
| --- | --- |
| `Graphics/Light/LightLayer.h` | `LightLayer`: darkens what was drawn to ambient plus the lights; `LightFrame` describes them |
| `Graphics/Paint/PaintStyle.h` | `LightSource`, `LightCone`: one light as plain data |
| `Graphics/Light/LightOccluders.h` | `LightOccluders`: what stops light, as outlines and lines |
| `Graphics/Light/CollisionOccluders.h` | `AddOccluders`: colliders as occluders, for walls |
| `Graphics/Paint/Painter.h` | `Painter::Light`: one light drawn directly, for your own light images |
| `Animation/Flicker.h` | `Flicker`, `FlickerStyle`: a lamp's strength over time |
| `Fx/FxStage.h`, `Fx/FxCue.h` | `FxLightFlash`, `FxStage::LightFlashes`: short lights |
| `Utilities/Geometry/Visibility.h` | `VisibilityPolygon`: what a point can see past edges, for a guard's view or your own light |
| `Graphics/Light/Debug/LightGuides.h` | `DrawLightGuides`: where each light stands, how far it reaches, what stops it |
| `Stage/Stage.h` | `StageSettings::light`: a scene's layer in its place in the frame, with still occluders and flashes |

## How a frame goes

In a scene, a [Stage](Stage.md) with `StageSettings::light` keeps this order for every view: the scene gives the ambient,
the lights and what moves in `StageView::DescribeLight` and what shines by itself in `DrawGlow`; the stage adds its still
occluders and the effects' flashes. The rest of this page is what happens underneath, and how to use the layer alone.

```
shown colour = world colour x (ambient + the sum of the lights), each channel at most 1
bloom        = not darkened
```

```cpp
const CameraView view = camera.View();
{
    auto world = paint.Camera(view);
    DrawWorld(paint);
}
const std::array lights{LightSource{.at = torch, .radius = 260.0f, .color = warm}};
_lights.Draw(paint, {.ambient = {18, 20, 34, 255}, .lights = lights}, view); // A LightLayer member
{
    auto world = paint.Camera(view);
    DrawNeon(paint); // Shines by itself
}
DrawHud(paint);
```

The layer adds the lights up in an image of their own, then multiplies what was drawn so far by that image. So:

- **Order is the tool.** What is drawn before the layer gets lit; what is drawn after it does not. A HUD goes after.
- **Lights are described every frame**, as plain data. Nothing is added or removed, and a split screen calls `Draw` once for
  each view with the same frame. A view with a viewport darkens only that part of the target.
- **Light reveals, it does not brighten.** A pixel never gets brighter than the colour it was drawn with. Overlapping lights
  stop adding up at full strength, per channel.
- **Call it outside `paint.Camera` and `paint.Transform`**, and give it the view the world was drawn through, shake
  included. Inside a camera it is an error (see [Painter's error policy](Painter.md)).

## A light

```cpp
LightSource{.at = position,                // Where it stands, in the coordinates of the view
            .radius = 300.0f,              // Where it reaches zero
            .color = {255, 200, 140, 255}, // Alpha dims it
            .intensity = 0.8f,             // Multiplies the colour; above 1 saturates its channels instead of growing
            .core = 0.2f,                  // Share of the radius that stays at full strength
            .cone = {.direction = angle, .halfAngle = 0.4f, .softEdge = 0.15f}}; // Omit for a whole circle
```

A pulse is a value you compute: a sum of sines, a [tween](../Animation.md), a clip. Move a light with the position the view
draws (`StepSmoothed::At(alpha)`).

## Flicker

```cpp
.intensity = Flicker(paint.Time(), FlickerStyle::Candle(), seed, game.Effects().flashes)
```

`Flicker` (`Animation/Flicker.h`) computes a strength in 0..1 from the time alone. It keeps nothing, so a light stays plain data,
a paused view stays still and a seek lands on the right look. `seed` keeps lamps apart. `FlickerStyle::Candle` wanders;
`DeadFluorescent` also has stretches of hard cuts, and every number of a `FlickerStyle` is yours to change.

The last argument is the player's choice, and it is required, because a forgotten check would keep flickering for someone who
turned flashes off, and no test would notice. Without flashes nothing cuts out and the wander slows to one change a second: a
candle sways and a failing tube burns steadily. It works without an `FxStage`.

## Short flashes

An explosion or lightning is a cue, and the frame takes what is playing:

```cpp
FxCue blast{FxLightFlash{.radius = 300.0f, .seconds = 0.25f}, FxShake{0.5f}}; // At FxPlay::at
FxCue lightning{FxLightFlash{.seconds = 0.4f}};                               // No radius: the whole scene

_lights.Draw(paint, {.ambient = dark, .lights = lights, .occluders = sets, .flashes = _fx.LightFlashes()}, view);
```

A flash with a radius is a light, so walls cast its shadows. One without raises the ambient, so nothing stops it. Effects know
light only as data: `Fx` hands over a `LightFlashFrame`, and neither folder includes the other. Forgetting the field shows no
flash at all, which is visible and harmless.

They follow [the effects' rules](Effects.md#graphics-and-fx): game time, so a hitstop holds them; `FxPlay::intensity` scales
the strength and leaves the time; flashes off removes them. Starts are rationed with `FxFlash`, at three a second in all, or the
two taking turns would double it. A flash and a light flash starting together count as one, and one asked for too soon is lost.
So lightning strikes once rather than twice in a tenth of a second, and of ten explosions in a second, three give light while
their particles and shake still play.

`ambient` is the light everything gets. White changes nothing, so a day-night cycle can reach it and the layer then costs
nothing. Fade the whole effect with `paint.Opacity` around `Draw`.

## Shadows

A `LightFrame` takes sets of `LightOccluders`. Keep one for what stands still, fill another for what moves:

```cpp
LightOccluders _walls; // Member: built when the level starts
AddOccluders(_walls, world, MaskOf(Layer::Wall));
```
```cpp
LightOccluders moving;
moving.Outline(playerOutline, Affine2::Translate(smoothedPlayer)); // Where it is drawn
const std::array<const LightOccluders*, 2> occluders{&_walls, &moving};
_lights.Draw(paint, {.ambient = night, .lights = lights, .occluders = occluders}, view);
```

**Colliders do not stop light by themselves.** The two answer different questions, and often differently:

| | Stops a player | Stops light |
| --- | --- | --- |
| A wall | Yes | Yes: `AddOccluders` |
| Glass, water, a trigger, a pickup, a bullet | Yes, or it reports a hit | No: leave them out of the mask |
| A background pillar, a tree | No | Yes: `Outline` |
| A character, whose hitbox is forgivingly small | Its hitbox | Its whole outline: `Outline`, or the shadow is smaller than the sprite |

`AddOccluders` is one way: it reads a world and changes nothing. Do not add a collision layer only for light, since layers are
part of a simulation's state and a look would change the replay hash. Use it for what stands still: a collider stands at the
last step while the view draws between steps, so a shadow from a moving collider lags behind it.

How light meets an outline:

- **`selfLit` (the default):** its near side and inside are lit, and the shadow starts behind it. This suits a character or a
  wall seen from above. Without it, light stops at the near side and the inside stays dark.
- **A light inside an outline is not stopped by it.** A torch set into a wall still lights the room, and a player's lantern
  ignores the player's own outline.
- **A `Line` has no inside** and stops light from both sides.
- **Nothing leaks** between shapes that touch or overlap.
- **`.shadows = false`** on a light makes it pass through everything.

`LightLayerOptions::softness` (default 8) is how far a shadow's edge spreads to each side, in world units; 0 leaves the steps of
the image's pixels. The whole light image is blurred, so a thin wall lets that much light past it, and the spread is the same
near an object and far from it. An outline need not be convex and has no limit on its points, unlike a collision `Shape`.

## What shines by itself

The layer does not touch bloom, so an emissive shape keeps its halo in the dark. Its own colour is multiplied like
everything else, so draw it **after** the layer to keep it at full colour. The price is order: it then lies over everything
drawn before, walls and characters included.

## Glow or light

| | `Painter::Glow`, `GlowStyle` | `LightSource` |
| --- | --- | --- |
| For | Decoration | What the player needs to see |
| Quality setting | `RenderQuality::glow` weakens or removes it | Ignored: there is no setting that hides the world |
| Does | Adds to what is below | Decides how much of what is below shows |

## Cost and resolution

Each `Draw` is one extra pass and one full-target image; each light is one quad, or a fan when something stops it, all in one
draw call. `LightLayerOptions::resolution` sizes the light image against the target. By default (0) it is half the target,
since lights are soft, half is rarely visible and it costs a quarter of the pixels; inside a pixel view it is the view's own
resolution, one light value per art pixel ([Pixel art](PixelArt.md#the-pixel-world)).

A shadowed light costs CPU, growing with the square of the edges within its reach, and edges out of reach cost almost nothing.
Measured on one desktop in an optimized build, with walls placed at random: about 10 µs per light at 25 edges, 40 µs at 50, 160
µs at 100, 630 µs at 200. So 20 lights among 50 walls' edges take under a millisecond. Give a small light a small radius, and
turn `.shadows` off on those that do not need it.

Softening takes two more passes over the light image, whatever the number of lights, and a second image of the same size. A
spread over about eight device pixels takes two more. A frame in which no shadow falls takes none of it, and a layer that never
has a shadow never allocates the image. Measured on one desktop GPU with six shadowed lights at 1280 x 720: about 0.03 ms a frame.

## Lighting decides nothing

Lighting is view code. A simulation cannot include it, and nothing reads the light image back. "Can the guard see the
player" belongs in the simulation, asked with `CollisionWorld::Raycast`.

## Checking a light

`DrawLightGuides` over the scene shows each light's place, reach, core and cone, and every occluder's edges. The editor's **Light Gallery** scene
(`engine.light.gallery`) shows every feature. `EngineGpuPixelCheck` reads back what the GPU really drew
([Testing](../Contributing/Testing.md#what-the-gpu-really-draws)).
