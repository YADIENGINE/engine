# Collision

Overlap tests, rays, sweeps and area queries for 2D and 2.5D arcade games. This is collision detection, not a physics
engine: nothing moves, bounces or falls by itself. The rules decide what a hit means. Box2D does the geometry behind this
API, and its names reach no public header. Design record: `Docs/Design/README.md` → D27 (engine repository).

**Guarantee:** the same as a [simulation](Simulation.md): the same build on the same machine. Debug and Release gave
identical hashes when measured, but that is not promised across compilers or CPUs.

## The parts

| Header | Use |
| --- | --- |
| `Physics/Collision/Shape.h` | `Shape`: circle, box, capsule, segment, convex polygon, in its own coordinates |
| `Physics/Collision/Pose.h` | `Pose`: position and angle in radians, clockwise on screen |
| `Physics/Collision/Collision.h` | Questions about two shapes, with nothing to set up |
| `Physics/Collision/CollisionWorld.h` | Many colliders: area, point, ray and sweep queries, overlapping pairs |
| `Physics/Collision/CollisionMask.h` | `MaskOf`: layers from the game's own enum |
| `Physics/Debug/ColliderDraw.h` | `DrawColliders`: outlines on top of a view |

Worked example: [AgentDocs/Recipes/Collision.md](../AgentDocs/Recipes/Collision.md).

## Two shapes

```cpp
if (ShapesOverlap(Shape::Circle(8.0f), {player}, Shape::Circle(6.0f), {coin})) ++score;
```

| Function | Answer |
| --- | --- |
| `ShapesOverlap`, `ShapeContains` | Whether they touch; touching counts |
| `ShapeDistance` | The gap between the surfaces, 0 when they overlap |
| `ShapeContact` | How they overlap: moving B by `normal * depth` separates them |
| `CastRay`, `CastShape` | Where a ray or a moving shape first touches the target |

A `Shape` is a plain value; keep constants of it. Shapes are convex, and a polygon has at most 8 corners, so split anything
else into several. Invalid sizes throw `std::invalid_argument`.

## Many colliders

`CollisionWorld` is an index. The game keeps its own positions, and calls `Move` when they change:

```cpp
enum class Layer { Player, Enemy, Wall };

const ColliderId id = world.Add(Shape::Box(16.0f, 3.0f), {position, angle}, MaskOf(Layer::Enemy), enemyIndex);
world.Move(id, {newPosition, angle});

for (const ColliderRef& found : world.Overlapping(blast, {center}, MaskOf(Layer::Enemy))) Damage(found.owner);
if (const auto hit = world.Raycast(from, to, MaskOf(Layer::Enemy, Layer::Wall))) DrawLaserTo(hit->point);
for (const ColliderPair& pair : world.Pairs(MaskOf(Layer::Player), MaskOf(Layer::Enemy))) Touch(pair.second.owner);
```

- **Fast objects:** use `Sweep`, and move only if it reports nothing. A shape cannot pass through a collider this way,
  however fast it goes. A bullet does not need to be a collider itself. A swept shape does not turn while it moves.
- **`owner`:** the number you gave to `Add`, usually an index into the game's own list.
- **Order:** results come in id order, and equal distances go to the lower id, so they do not depend on the index's inner
  shape.
- **Spans:** a result stays valid until the next query on that world. Copy what must last longer.
- **Ids:** a removed collider's id stays invalid, even after its slot is used again. Using one throws.
- **A collider asking about itself:** pass its id as `ignore`, or it finds itself.

## Walls that stop light

`AddOccluders` (`Graphics/Light/CollisionOccluders.h`) gives a light layer the colliders in a mask. It only reads the world, and
colliders stop no light unless you ask: [Lighting](Graphics/Lighting.md#shadows).

## In a simulation

`engine_add_simulation` links collision, so a simulation only includes the headers. Keep the world as a member and include
it in `State`:

```cpp
state.Field("collision", _collision);
```

`sim state` then lists every collider, as `collision.collider[N].position.x` and so on. A world is not a hidden input, because
everything it knows came through calls the tape reproduces.

A forgotten `Move` is a bug hashes cannot catch: it replays the same way every time. It shows as a collider left behind its
owner, in `sim state` beside the game's own position, or on screen with `DrawColliders`.

## Seeing colliders

`DrawColliders(paint, world)` outlines every collider, one color per layer, so what the rules see can be compared with what
the view draws. Style: line width and which layers to show. It is for views only: a simulation cannot include it, and
`python Engine check` says so.

## Costs

Measured on one machine in Release, per step, at 60 steps per second:

| Case | Cost |
| --- | --- |
| 1,000 / 5,000 / 20,000 moving objects, each asking what is near, evenly spread | 0.8 / 8 / 105 ms |
| 1,000 / 5,000 of them packed so that nearly everything overlaps | 4 / 100 ms |
| 500 / 2,000 / 5,000 bullets sweeping against 100 / 500 / 2,000 moving enemies | 0.1 / 0.6 / 2.8 ms |

A step has about 16 ms. Prefer `Sweep` from bullets over making every bullet a collider and asking `Pairs`. Colliders that do
not move cost nothing per step, and `Move` with an unchanged pose does no work. A source build in Debug is several times
slower; a project on the prebuilt engine links the optimized library in Debug as well.

## What it does not do

No gravity, no bounce, no forces and no joints. No "contact started" or "contact ended" events; compare `Pairs` between steps
if a rule needs them. No sliding along walls yet: `ShapeContact` gives what is needed to push a shape out by hand. Box2D's
physics stays in `ThirdParty/Box2D`, unused.
