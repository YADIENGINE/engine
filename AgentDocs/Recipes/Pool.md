# Pool recipe

Keep things the rules spawn and remove in a `Pool`, and point at them with handles: a homing missile keeps its target as
a `Handle`, finds it while it lives, and picks another once it is gone. Inside a simulation that replays exactly.

APIs: Core/Utilities/Pool.h. With colliders: Core/Physics/Collision/PoolColliders.h, shown in the
[collision recipe](Collision.md). Concepts: [Simulation](../../Docs/Simulation.md#things-that-come-and-go).

Put the rules in Game/Simulation/: `Pool` is header-only, so including it is enough. Keep the drawing function outside
that folder.

<!-- recipe:PoolTargets-header -->
```cpp
#pragma once
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRegistry.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Pool.h"
#include "Utilities/Random/Rng.h"

class Painter;

enum class RecipeHomingButton { Fire };

// Things the rules spawn and remove are plain structs kept in a Pool, so each has a handle that lasts.
struct RecipeDrone {
    Float2 at;
    float drift = 0.0f; // Sideways, units per second
    int hp = 2;
};
struct RecipeMissile {
    Float2 at;
    Handle<RecipeDrone> target; // Not an index: it finds its drone while it lives, and nothing after
    bool spent = false;
};
// Everything that decides the game, visited whole: each pool writes its items with their handles, and its free slots.
struct RecipeHomingRun {
    Rng rng;
    Pool<RecipeDrone> drones;
    Pool<RecipeMissile> missiles;
    int score = 0;
};

// The rules, in Game/Simulation: Fire launches a missile at the nearest drone; a missile whose drone is gone picks
// another.
class RecipeHomingSimulation final : public Simulation {
public:
    explicit RecipeHomingSimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;

    [[nodiscard]] const RecipeHomingRun& Run() const { return _run; }

private:
    RecipeHomingRun _run;
};

[[nodiscard]] SimulationDefinition RecipeHomingDefinition();
// The view, outside Game/Simulation: a line from each missile to its target.
void DrawRecipeHoming(Painter& paint, const RecipeHomingSimulation& simulation);
```
<!-- /recipe -->

<!-- recipe:PoolTargets -->
```cpp
#include "AgentDocs/Recipes/Code/PoolTargets.h"
#include <limits>
#include <memory>
#include "Graphics/Paint/PaintStyle.h"
#include "Graphics/Paint/Painter.h"
#include "Simulation/StateVisitor.h"

namespace {
constexpr float FieldWidth = 800.0f;
constexpr Float2 Launcher{400.0f, 560.0f};
constexpr float MissileSpeed = 300.0f;
constexpr float HitRadius = 14.0f;
constexpr std::size_t DroneCount = 5;
constexpr Color4 DroneColor{255, 110, 120, 255};
constexpr Color4 MissileColor{255, 230, 120, 255};
constexpr Color4 AimColor{255, 230, 120, 90};

// The nearest drone, or the null handle when there is none. Equal distances go to the earlier drone: spawn order.
Handle<RecipeDrone> Nearest(const Pool<RecipeDrone>& drones, Float2 from) {
    Handle<RecipeDrone> nearest;
    float best = std::numeric_limits<float>::max();
    for (const RecipeDrone& drone : drones) {
        if (const float distance = LengthSquared(drone.at - from); distance < best) {
            best = distance;
            nearest = drones.HandleOf(drone);
        }
    }
    return nearest;
}
} // namespace

RecipeHomingSimulation::RecipeHomingSimulation(const SimulationStart& start)
    : _run{.rng = Rng(start.seed)} {}
void RecipeHomingSimulation::Step(const SimulationStep& step) {
    // Spawning and firing happen outside any loop over the pool they add to.
    while (_run.drones.Count() < DroneCount)
        (void)_run.drones.Add({.at = {_run.rng.Float(40.0f, FieldWidth - 40.0f), _run.rng.Float(60.0f, 260.0f)},
                               .drift = _run.rng.Float(-60.0f, 60.0f)});
    if (step.Player(0).Pressed(RecipeHomingButton::Fire))
        (void)_run.missiles.Add({.at = Launcher, .target = Nearest(_run.drones, Launcher)});

    for (RecipeDrone& drone : _run.drones) {
        drone.at.x += drone.drift * step.seconds;
        if (drone.at.x < 0.0f || drone.at.x > FieldWidth) drone.drift = -drone.drift;
    }
    for (RecipeMissile& missile : _run.missiles) {
        if (!_run.drones.Contains(missile.target)) missile.target = Nearest(_run.drones, missile.at); // Gone: retarget
        RecipeDrone* target = _run.drones.Find(missile.target); // Used now, never kept
        if (target == nullptr) {
            missile.at.y -= MissileSpeed * step.seconds;
            missile.spent = missile.at.y < 0.0f;
            continue;
        }
        missile.at += Normalized(target->at - missile.at) * (MissileSpeed * step.seconds);
        if (Distance(missile.at, target->at) < HitRadius) {
            target->hp -= 1;
            missile.spent = true;
        }
    }
    // Removal after the loops, once per step; the others keep their order and their handles.
    (void)_run.missiles.RemoveIf([](const RecipeMissile& missile) { return missile.spent; });
    _run.score += static_cast<int>(_run.drones.RemoveIf([](const RecipeDrone& drone) { return drone.hp <= 0; }));
}
// drones[0].handle.index, drones[0].hp, missiles[0].target.index, drones.free[0], ...
void RecipeHomingSimulation::State(StateVisitor& state) const { state.Fields(_run); }

SimulationDefinition RecipeHomingDefinition() {
    SimulationDefinition definition;
    definition.settings.id = "myproject.homing";
    definition.settings.buttons = {"Fire"};
    definition.create = [](const SimulationStart& start) { return std::make_unique<RecipeHomingSimulation>(start); };
    return definition;
}

void DrawRecipeHoming(Painter& paint, const RecipeHomingSimulation& simulation) {
    const RecipeHomingRun& run = simulation.Run();
    for (const RecipeDrone& drone : run.drones)
        paint.Circle(drone.at, 6.0f + 3.0f * static_cast<float>(drone.hp), Fill(DroneColor));
    for (const RecipeMissile& missile : run.missiles) {
        if (const RecipeDrone* target = run.drones.Find(missile.target))
            paint.Line(missile.at, target->at, {.width = 1.0f, .color = AimColor});
        paint.Circle(missile.at, 4.0f, Fill(MissileColor));
    }
}
```
<!-- /recipe -->

Test it without a window: `python Engine sim run scenario.json`, with the scenario below. Both missiles aim at drone
slot 3; when it dies, its slot is free and its generation has moved on, so the next drone there gets a new handle.

```json
{ "simulation": "myproject.homing", "seed": 3, "script": [
  { "run": 1 }, { "expect": { "drones.count": 5, "missiles.count": 0 } },
  { "tap": "Fire" }, { "run": 3 }, { "tap": "Fire" }, { "run": 3 },
  { "expect": { "missiles.count": 2, "missiles[0].target.index": 3, "missiles[1].target.index": 3 } },
  { "until": { "score": 1 }, "limit": 300 },
  { "expect": { "drones.count": 4, "drones.free[0]": 3, "drones.generations[3]": 2 } } ] }
```

Checks:

- A stored reference to another thing is a `Handle`, never an index into a list or a pointer: removing other items does
  not change it, and a removed item's handle finds nothing.
- `Find` gives a pointer to use at once; keep the handle, not the pointer.
- Nothing is added or removed inside a loop over the same pool: it throws. Add before or after the loop, and remove with
  one `RemoveIf` per step.
- `sim state` lists each item with its handle (`drones[0].handle.index`) and the pool's free slots (`drones.free[0]`).
