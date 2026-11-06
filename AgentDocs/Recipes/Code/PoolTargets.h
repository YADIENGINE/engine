// [recipe:PoolTargets-header]
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
// [/recipe]
