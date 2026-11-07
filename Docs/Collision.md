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
| `Physics/Collision/PoolColliders.h` | `PoolColliders`: how a [pool](Simulation.md#things-that-come-and-go)'s items have colliders, declared once |
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

const ColliderId id = world.Add(Shape::Box(16.0f, 3.0f), {position, angle}, MaskOf(Layer::Enemy), owner);
world.Move(id, {newPosition, angle});

for (const ColliderRef& found : world.Overlapping(blast, {center}, MaskOf(Layer::Enemy))) Damage(found.owner);
if (const auto hit = world.Raycast(from, to, MaskOf(Layer::Enemy, Layer::Wall))) DrawLaserTo(hit->point);
for (const ColliderPair& pair : world.Pairs(MaskOf(Layer::Player), MaskOf(Layer::Enemy))) Touch(pair.second.owner);
```

- **Fast objects:** use `Sweep`, and move only if it reports nothing. A shape cannot pass through a collider this way,
  however fast it goes. A bullet does not need to be a collider itself. A swept shape does not turn while it moves.
- **`owner` and `layers`:** every result carries the number you gave to `Add` and the collider's layers. For things that
  spawn and die, let `PoolColliders` set the owner and read it back ([below](#items-in-a-pool)). Do not use an index into
  a list as owner: removing an earlier item makes it name the wrong one.
- **Order:** results come in id order, and equal distances go to the lower id, so they do not depend on the index's inner
  shape.
- **Spans:** a result stays valid until the next query on that world. Copy what must last longer.
- **Ids:** a removed collider's id stays invalid, even after its slot is used again. Using one throws.
- **A collider asking about itself:** pass its id as `ignore`, or it finds itself.

## Items in a pool

When the things with colliders spawn and die, keep them in a [`Pool`](Simulation.md#things-that-come-and-go), each with
its `ColliderId`. Declare once, beside the item type, how its items have colliders: which member holds the id, where the
item stands, and the pool's layers. That `PoolColliders` value is compile-time data; it points at no world or pool. Every
call goes through it, so the owner is always the item's handle and the layers always the pool's:

```cpp
struct Enemy { Pose pose; int hp = 2; ColliderId collider; };
inline constexpr PoolColliders EnemyColliders{&Enemy::collider, &Enemy::pose, MaskOf(Layer::Enemy)};

EnemyColliders.Add(_world, _enemies, {.pose = spawn}, Shape::Box(16.0f, 3.0f)); // Item and collider, at its pose
EnemyColliders.Sync(_world, _enemies); // Each step, after the rules moved the items: no Move to forget
if (const auto hit = _world.Sweep(bullet, from, move, EnemyColliders.Layers() | MaskOf(Layer::Wall)))
    if (Enemy* enemy = _enemies.Find(EnemyColliders.Of(*hit))) enemy->hp -= 1; // Null for the wall
EnemyColliders.RemoveIf(_world, _enemies, [](const Enemy& e) { return e.hp <= 0; }); // Colliders leave too
```

- **Where it stands:** a member `Pose` or `Float2` (no turn), or a lambda without captures:
  `[](const Enemy& e) { return Pose{e.at, e.angle}; }`. An unchanged pose costs a comparison in `Sync`.
- **Back from a hit:** `Of` takes a `ColliderRef` (each side of a `ColliderPair` too) or a `ColliderHit`, and gives the
  item's handle, or null when the collider is on none of the pool's layers. The item may be gone since: use `Find`.
- **Layers:** give each pool its own. Two pools may give equal handles, as two lists give equal indices; a pool may have
  several layers, and `Of` accepts a collider on any of them.
- **No collider:** an item whose collider is `{}` (added with `Pool::Add`, or whose collider the game removed and cleared)
  is skipped. `Remove` removes one item and its collider by handle.

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
owner, in `sim state` beside the game's own position, or on screen with `DrawColliders`. Items in a pool need one call per
step instead: `PoolColliders::Sync` moves them all.

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
