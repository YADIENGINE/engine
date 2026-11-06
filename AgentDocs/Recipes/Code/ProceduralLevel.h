// [recipe:ProceduralLevel-header]
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "Simulation/HostedSimulation.h"
#include "Editor/Api/SceneRegistry.h"
#include "Scene/Scene.h"
#include "Input/InputBindings.h"
#include "Procedural/MapCell.h"
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRegistry.h"
#include "Utilities/Geometry/Grid.h"
#include "Utilities/Random/DrawTable.h"
#include "Utilities/Random/Rng.h"

class Painter;

enum class RecipeDungeonButton { Left, Right, Up, Down };

// Everything that decides the run, in one plain struct: State visits it whole, so the generated map, the scattered
// gems, the loot table's pity counters and the generator all feed the replay hash.
struct RecipeDungeonRun {
    Rng rng;                // Loot rolls during play
    Grid<MapCell> map;      // Generated from the seed; kept in the state so the rules may change it (doors, digging)
    std::vector<Vec2> gems; // Scattered over the floor
    Vec2 start;
    Vec2 player;
    Float2 wisp; // A ghost that drifts on smooth noise; touching it sends the player back to the start
    DrawMemory loot;
    std::vector<int> found; // How often each loot entry came up
    int moveWait = 0;       // Steps until the player may move again
    int age = 0;
};

// The rules, in Game/Simulation. start.parameters["layout"] picks "rooms", "caves" or "walk".
class RecipeDungeonSimulation final : public Simulation {
public:
    explicit RecipeDungeonSimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;

    [[nodiscard]] const RecipeDungeonRun& Run() const { return _run; }

private:
    RecipeDungeonRun _run;
};

[[nodiscard]] SimulationDefinition RecipeDungeonDefinition();
// The loot a gem gives: coins, a potion, a key, and a crown that comes at the latest on the 16th gem.
[[nodiscard]] const DrawTable& RecipeDungeonLoot();
// The view, outside Game/Simulation: smooth noise shades the floor, which decides nothing.
void DrawRecipeDungeon(Painter& paint, const RecipeDungeonSimulation& simulation);

// The scene, also in the editor: `ctl open myproject.dungeon --set layout=caves --set seed=7 --screenshot`.
class RecipeDungeonScene final : public Scene {
public:
    RecipeDungeonScene(const SceneServices& services, std::uint64_t seed, const std::string& layout);
    ~RecipeDungeonScene() override { ShutdownNoThrow(); }

private:
    void HandleInput(const DeviceInput& input) override;
    void InputCancelled() override;
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;
    HostedSimulation<RecipeDungeonSimulation> _hosted;
    InputBindings _bindings;
};
[[nodiscard]] SceneDefinition RecipeDungeonSceneDefinition();
// [/recipe]
