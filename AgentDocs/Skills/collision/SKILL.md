---
name: collision
description: Add or debug collision - hits, overlaps, hit boxes, walls, lasers, fast bullets, layers, what is in an area or under a point. Use also for bugs such as "goes through", "did not hit", "hits from too far". Not a physics engine - no gravity or bounce.
---

# Add or debug collision

Reference: `Docs/Collision.md`. Example that compiles: `AgentDocs/Recipes/Collision.md`. Includes start at `Physics/Collision/`;
a simulation needs no CMake line. Never include Box2D.

## Choose the call

| The rule asks | Use |
| --- | --- |
| Does this one thing touch that one thing? | `ShapesOverlap`, with no world at all |
| Did a bullet or anything fast hit something? | `world.Sweep`, then move only if it found nothing |
| What is inside this blast or area? | `world.Overlapping` |
| What does this laser reach first? | `world.Raycast` |
| Who touches whom this step? | `world.Pairs(MaskOf(...), MaskOf(...))` |
| What is under this point? | `world.At` |
| How do I push this out of that wall? | `ShapeContact`: move B by `normal * depth` |

Bullets usually need no collider: they only ask, with `Sweep`. Give a collider to what others must find.

## Write it

1. Declare layers as one enum, and pass them through `MaskOf`.
2. `Add` each collider with its `owner`, usually an index into the game's own list. Keep the returned id.
3. Call `Move` wherever the game changes that position. The game owns positions; the world knows only what it is told.
4. In a simulation, keep the world as a member and add it to `State`:
   `state.Field("collision", _collision);`
5. Test without a window: the [simulation skill](../simulation/SKILL.md). Check that a hit happens and also that a near miss
   does not.

<!-- recipe:SkillCollision -->
```cpp
enum class Layer { Player, Enemy };

// Inside simulation rules. The world is a member; positions stay in the game's own fields.
class Rules {
public:
    Rules() { _player = _world.Add(Shape::Circle(12.0f), {_playerPosition}, MaskOf(Layer::Player)); }

    void Step(float seconds) {
        _playerPosition.x += 240.0f * seconds;
        _world.Move(_player, {_playerPosition}); // Wherever the game changes a position
        const Float2 move{0.0f, -3000.0f * seconds};
        std::erase_if(_bullets, [&](Float2& bullet) { // Fast things ask with Sweep and move only on a miss
            const auto hit = _world.Sweep(Shape::Circle(4.0f), {bullet}, move, MaskOf(Layer::Enemy));
            if (!hit) {
                bullet += move;
                return false;
            }
            _world.Remove(hit->id); // hit->owner is what Add was given: an index into the game's list
            return true;
        });
    }
    void State(StateVisitor& state) const { state.Field("collision", _world); }

private:
    CollisionWorld _world;
    ColliderId _player;
    Float2 _playerPosition{400.0f, 500.0f};
    std::vector<Float2> _bullets; // Bullets only ask, so they need no collider
};
```
<!-- /recipe -->

## Pitfalls

- **A result span lasts until the next query on that world.** A query inside a loop over a span breaks it. Copy first, or
  collect what to do and act after the loop.
- **Removing inside a loop over results:** the same collider can appear twice in `Pairs`, and removing it twice throws. Check
  `Contains` first, or collect removals.
- **A collider asking about itself finds itself.** Pass its id as `ignore`.
- A sweep that starts inside something hits at fraction 0 with a zero normal. Do not bounce off that normal.
- A swept shape does not turn while it moves. Touching counts as overlapping.
- Polygons are convex with at most 8 corners: split larger outlines. Invalid shapes, ids and poses throw.
- Angles are radians, clockwise on screen; y grows downwards. `Shape::Box` takes half sizes.

## A hit that goes wrong

Work in this order, because each step rules out a whole kind of bug:

1. **Reproduce it** with a scenario that ends just before the moment, then `read` the fields. Output field names are those of
   `python Engine sim state TAPE --step N`.
2. **A forgotten `Move`:** compare the game's own position with `collision.collider[N].position.x`. If they differ, a `Move` is
   missing. Hashes and `sim verify` cannot show this: it replays the same way every time.
3. **Layers:** `collision.collider[N].layers` holds `1 << enum value`. Compare it with the mask the query uses.
4. **Too fast:** a position changed without a `Sweep` can jump over a thin collider between steps.
5. **Wrong size or offset:** look at it, with an open editor (`AgentDocs/Agent.md`), or ask the developer to. Call `DrawColliders(paint, world)` from the view behind a parameter, then
   `python Engine ctl open ID --set showColliders=true --screenshot --scale 0.5`. Outlines that disagree with the art are the
   bug. `python Engine ctl seek --step N` reaches the moment. Only a view can draw them: `python Engine check` refuses that
   include inside `Game/Simulation`.

Report which step found it, and add the scenario that failed as a test.

## Cost

A step has about 16 ms. Thousands of bullets sweeping against moving enemies cost a few ms; measured numbers are in
`Docs/Collision.md#costs`. Colliders that do not move cost nothing per step, so do not rebuild a world each step.
