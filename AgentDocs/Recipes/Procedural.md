# Procedural recipe

Generate a level from the seed (rooms, caves or a random walk), scatter gems over its floor, give loot from a table with
pity, and drift a ghost on smooth noise, inside a simulation that replays exactly.

APIs: Core/Procedural/Rooms.h, Core/Procedural/Caves.h, Core/Procedural/RandomWalk.h, Core/Procedural/Regions.h, Core/Utilities/Geometry/Grid.h, Core/Utilities/Random/Scatter.h, Core/Utilities/Random/DrawTable.h, Core/Utilities/Random/NoiseField.h. Concepts: [Procedural](../../Docs/Procedural.md).

Put the rules in Game/Simulation/: `engine_add_simulation` already links the generators, so including the headers is enough. Keep the drawing function outside that folder.

<!-- recipe:ProceduralLevel-header -->
```cpp
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
```
<!-- /recipe -->

<!-- recipe:ProceduralLevel -->
```cpp
#include "AgentDocs/Recipes/Code/ProceduralLevel.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>
#include "Graphics/Paint/PaintStyle.h"
#include "Graphics/Paint/Painter.h"
#include "Input/Input.h"
#include "Procedural/Caves.h"
#include "Procedural/RandomWalk.h"
#include "Procedural/Regions.h"
#include "Procedural/Rooms.h"
#include "Simulation/StateVisitor.h"
#include "Utilities/Random/NoiseField.h"
#include "Utilities/Random/Scatter.h"

namespace {
constexpr Vec2 MapSize{48, 27};
constexpr float GemSpacing = 4.0f; // Cells between gems, at least
constexpr int MoveSteps = 6;       // A held button moves one cell every 6 steps
constexpr NoiseSettings WispDrift{.frequency = 0.004f, .fractal = NoiseFractal::FBm, .seed = 3};
constexpr float WispSpeed = 0.05f; // Cells per step at full drift
constexpr NoiseSettings FloorShade{.frequency = 0.15f, .seed = 9};
constexpr float CellSize = 26.0f;

// The layout the start asks for, from a stream of its own, so the loot rolls stay the same whatever layout is used.
Grid<MapCell> Generate(const SimulationStart& start, Rng& layout) {
    const auto& members = start.parameters.Members();
    const auto found = members.find("layout");
    const std::string kind = found != members.end() && std::holds_alternative<std::string>(found->second.value)
                                 ? found->second.AsString()
                                 : "rooms";
    if (kind == "caves") return GenerateCaves({.size = MapSize}, layout);
    if (kind == "walk") return GenerateRandomWalk({.size = MapSize, .floorShare = 0.38f}, layout);
    return GenerateRooms({.size = MapSize, .minRoom = 4, .maxRoom = 9}, layout).cells;
}
} // namespace

const DrawTable& RecipeDungeonLoot() {
    static const DrawTable loot{.entries = {{.weight = 60.0},             // Coins
                                            {.weight = 30.0},             // Potion
                                            {.weight = 9.0},              // Key
                                            {.weight = 1.0, .pity = 15}}, // Crown: on the 16th gem at the latest
                                .noRepeat = true};
    return loot;
}

RecipeDungeonSimulation::RecipeDungeonSimulation(const SimulationStart& start) {
    _run.rng = Rng(start.seed);
    Rng layout(start.seed, 1);
    _run.map = Generate(start, layout);
    // The player starts on the floor cell nearest the top left: the largest area's first cell in a row-by-row scan.
    const auto regions = FloorRegions(_run.map);
    if (!regions.empty())
        _run.start = *std::ranges::min_element(regions.front(),
                                               [](Vec2 a, Vec2 b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
    _run.player = _run.start;
    _run.wisp = ToFloat2(MapSize) * 0.5f;
    // Gems spread evenly over the map; those that land on floor stay.
    for (const Float2 point : ScatterPoints(FloatRect::From(_run.map.Bounds()), GemSpacing, layout)) {
        const Vec2 cell{static_cast<int>(point.x), static_cast<int>(point.y)};
        if (_run.map[cell] == MapCell::Floor && cell != _run.start) _run.gems.push_back(cell);
    }
    _run.found.assign(RecipeDungeonLoot().entries.size(), 0);
}
void RecipeDungeonSimulation::Step(const SimulationStep& step) {
    const auto& input = step.Player(0);
    if (_run.moveWait > 0) --_run.moveWait;
    Vec2 move{};
    if (input.Held(RecipeDungeonButton::Left)) move = {-1, 0};
    if (input.Held(RecipeDungeonButton::Right)) move = {1, 0};
    if (input.Held(RecipeDungeonButton::Up)) move = {0, -1};
    if (input.Held(RecipeDungeonButton::Down)) move = {0, 1};
    const Vec2 next = _run.player + move;
    if (move != Vec2{} && _run.moveWait == 0 && _run.map.At(next, MapCell::Wall) == MapCell::Floor) {
        _run.player = next;
        _run.moveWait = MoveSteps;
    }
    // A gem gives what the loot table draws; the table's memory is in the state, so pity survives a replay.
    if (const auto gem = std::ranges::find(_run.gems, _run.player); gem != _run.gems.end()) {
        _run.gems.erase(gem);
        ++_run.found[RecipeDungeonLoot().Draw(_run.loot, _run.rng)];
    }
    // Smooth noise in the rules: precise floats, so a replay drifts the same way.
    const auto age = static_cast<float>(_run.age);
    const Float2 drift{Noise(WispDrift, Float2{age, 0.0f}), Noise(WispDrift, Float2{0.0f, age})};
    _run.wisp = {std::clamp(_run.wisp.x + drift.x * WispSpeed, 0.0f, ToFloat2(MapSize).x - 1.0f),
                 std::clamp(_run.wisp.y + drift.y * WispSpeed, 0.0f, ToFloat2(MapSize).y - 1.0f)};
    if (Distance(_run.wisp, ToFloat2(_run.player)) < 0.8f) _run.player = _run.start;
    ++_run.age;
}
void RecipeDungeonSimulation::State(StateVisitor& state) const { state.Fields(_run); }
SimulationDefinition RecipeDungeonDefinition() {
    SimulationDefinition definition;
    definition.settings.id = "myproject.dungeon";
    definition.settings.buttons = {"Left", "Right", "Up", "Down"};
    definition.create = [](const SimulationStart& start) { return std::make_unique<RecipeDungeonSimulation>(start); };
    return definition;
}

void DrawRecipeDungeon(Painter& paint, const RecipeDungeonSimulation& simulation) {
    const RecipeDungeonRun& run = simulation.Run();
    const auto at = [](Float2 cell) { return cell * CellSize; };
    for (int y = 0; y < run.map.Height(); ++y)
        for (int x = 0; x < run.map.Width(); ++x) {
            const Float2 corner = at(ToFloat2(Vec2{x, y}));
            if (run.map[{x, y}] == MapCell::Wall) {
                paint.Rect({corner.x, corner.y, CellSize, CellSize}, Fill(Color4{28, 30, 44, 255}));
                continue;
            }
            // View-only noise: the same cell always gets the same shade, with no state to keep.
            const float shade = Noise(FloorShade, ToFloat2(Vec2{x, y})) * 0.5f + 0.5f;
            const auto level = static_cast<std::uint8_t>(70.0f + shade * 40.0f);
            paint.Rect({corner.x, corner.y, CellSize, CellSize}, Fill(Color4{level, level, 90, 255}));
        }
    const Float2 middle{CellSize * 0.5f, CellSize * 0.5f};
    for (const Vec2 gem : run.gems)
        paint.Circle(at(ToFloat2(gem)) + middle, CellSize * 0.25f, Fill(Color4{120, 230, 255, 255}));
    paint.Circle(at(run.wisp) + middle, CellSize * 0.4f, Fill(Color4{235, 235, 255, 150}));
    paint.Circle(at(ToFloat2(run.player)) + middle, CellSize * 0.35f, Fill(Color4{255, 200, 90, 255}));
}

RecipeDungeonScene::RecipeDungeonScene(const SceneServices& services, std::uint64_t seed, const std::string& layout)
    : _hosted(services.simulation, RecipeDungeonDefinition().settings,
              [&] {
                  SimulationStart start{.seed = seed, .players = 1};
                  start.parameters["layout"] = layout;
                  return start;
              }())
    , _bindings(1) {
    _bindings.Bind(0, RecipeDungeonButton::Left, InputKey::A);
    _bindings.Bind(0, RecipeDungeonButton::Right, InputKey::D);
    _bindings.Bind(0, RecipeDungeonButton::Up, InputKey::W);
    _bindings.Bind(0, RecipeDungeonButton::Down, InputKey::S);
}
void RecipeDungeonScene::HandleInput(const DeviceInput& input) { _hosted.Runner().Feed(_bindings.Read(input)); }
void RecipeDungeonScene::InputCancelled() { _hosted.Runner().CancelInput(); }
void RecipeDungeonScene::Update(float seconds) { (void)_hosted.Runner().Advance(seconds); }
void RecipeDungeonScene::Draw(Painter& paint) const { DrawRecipeDungeon(paint, _hosted.Sim()); }
SceneDefinition RecipeDungeonSceneDefinition() {
    SceneDefinition definition;
    definition.id = "myproject.dungeon";
    definition.name = "Dungeon";
    definition.category = "MyProject";
    definition.description = "WASD moves; gems give loot, the wisp sends you back. Each seed is another level.";
    SceneParameter layout{"layout", "Layout", SceneParameterKind::Choice, std::string("rooms")};
    layout.apply = SceneApplyMode::OnReset;
    layout.choices = {"rooms", "caves", "walk"};
    definition.parameters = {
        {"seed", "Seed", SceneParameterKind::Integer, std::int64_t{1}, 0.0, 1000000.0, SceneApplyMode::OnReset},
        layout};
    definition.create = [](const SceneServices& services, const SceneParameters& parameters) {
        return std::make_unique<RecipeDungeonScene>(
            services, static_cast<std::uint64_t>(std::get<std::int64_t>(parameters.at("seed"))),
            std::get<std::string>(parameters.at("layout")));
    };
    return definition;
}
```
<!-- /recipe -->

See it: `python Engine ctl open myproject.dungeon --set layout=caves --set seed=7 --screenshot --scale 0.5`.

Checks:

- Generate in the constructor from `start.seed`, with an `Rng` stream of its own (`Rng(start.seed, 1)`), so changing the level does not move the rolls of play.
- The map, the gems, the table's `DrawMemory` and the generator are all in the state struct; `python Engine sim state` lists `map.cells[N]`, `loot.sinceDrawn[N]` and `rng.state`.
- Noise in the rules reads step-based inputs (`age`), never seconds. Noise in the view (the floor shade) decides nothing.
- A scattered point becomes a cell with `static_cast<int>`; keep only points that land on floor.
