// [recipe:PoolTargets]
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
// [/recipe]
