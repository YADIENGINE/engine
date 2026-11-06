# Collision recipe

Stop fast bullets, keep drifting enemies in a pool whose colliders follow them, find overlapping pairs and show colliders
on screen, inside a simulation that replays exactly.

APIs: Core/Physics/Collision/CollisionWorld.h, Core/Physics/Collision/Shape.h, Core/Physics/Collision/CollisionMask.h, Core/Physics/Collision/PoolColliders.h, Core/Utilities/Pool.h, Core/Physics/Debug/ColliderDraw.h. Concepts: [Collision](../../Docs/Collision.md).

Put the rules in Game/Simulation/: `engine_add_simulation` already links collision, so including the headers is enough. Keep the drawing function outside that folder.

<!-- recipe:CollisionBullets-header -->
```cpp
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "Simulation/HostedSimulation.h"
#include "Editor/Api/SceneRegistry.h"
#include "Scene/Scene.h"
#include "Input/InputBindings.h"
#include "Physics/Collision/CollisionWorld.h"
#include "Physics/Collision/PoolColliders.h"
#include "Physics/Collision/Pose.h"
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRegistry.h"
#include "Utilities/Pool.h"

class Painter;

enum class RecipeLayer { Player, Enemy, Wall };
enum class RecipeShooterButton { Left, Right, Fire };

// A plain struct, so State writes it field by field.
struct RecipeEnemy {
    Pose pose;
    float drift = 0.0f; // Sideways, units per second
    int hp = 2;
    ColliderId collider;
};
// Declared once beside the type: each enemy's collider is its `collider`, stands at its `pose`, is on the Enemy layer
// and is owned by its handle.
inline constexpr PoolColliders RecipeEnemyColliders{&RecipeEnemy::collider, &RecipeEnemy::pose,
                                                    MaskOf(RecipeLayer::Enemy)};

// The rules, in Game/Simulation: colliders are found through the world, positions stay in the game's own fields.
class RecipeShooterSimulation final : public Simulation {
public:
    explicit RecipeShooterSimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;

    [[nodiscard]] int Score() const { return _score; }
    [[nodiscard]] int Touches() const { return _touches; }
    [[nodiscard]] float PlayerX() const { return _playerX; }
    [[nodiscard]] const CollisionWorld& Collision() const { return _collision; }
    [[nodiscard]] const std::vector<Float2>& Bullets() const { return _bullets; }
    [[nodiscard]] const Pool<RecipeEnemy>& Enemies() const { return _enemies; }

private:
    CollisionWorld _collision;
    ColliderId _player;
    Pool<RecipeEnemy> _enemies;
    std::vector<Float2> _bullets; // Bullets only ask questions, so they need no collider of their own
    float _playerX = 400.0f;
    int _score = 0;
    int _touches = 0;
};

[[nodiscard]] SimulationDefinition RecipeShooterDefinition();
// The view, outside Game/Simulation. A debug switch shows what the rules see on top of what the player sees.
void DrawRecipeShooter(Painter& paint, const RecipeShooterSimulation& simulation, bool showColliders);

// The scene, with the switch as an editor parameter: `ctl open ID --set showColliders=true --screenshot`.
class RecipeShooterScene final : public Scene {
public:
    RecipeShooterScene(const SceneServices& services, std::uint64_t seed, bool showColliders);
    ~RecipeShooterScene() override { ShutdownNoThrow(); }

private:
    void HandleInput(const DeviceInput& input) override;
    void InputCancelled() override;
    [[nodiscard]] std::string ApplyParameter(const std::string& id, const SceneValue& value) override;
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;
    HostedSimulation<RecipeShooterSimulation> _hosted;
    InputBindings _bindings;
    bool _showColliders;
};
[[nodiscard]] SceneDefinition RecipeShooterSceneDefinition();
```
<!-- /recipe -->

<!-- recipe:CollisionBullets -->
```cpp
#include "AgentDocs/Recipes/Code/CollisionBullets.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <variant>
#include "Graphics/Paint/PaintStyle.h"
#include "Graphics/Paint/Painter.h"
#include "Physics/Debug/ColliderDraw.h"
#include "Simulation/StateVisitor.h"
#include "Utilities/Random/Rng.h"

namespace {
constexpr float FieldWidth = 800.0f;
constexpr float PlayerY = 500.0f;
constexpr float PlayerSpeed = 240.0f;
constexpr float BulletSpeed = 3000.0f; // 50 units per step: without a sweep it would jump over a thin enemy
constexpr float BulletRadius = 4.0f;
constexpr int EnemyCount = 8;
constexpr float EnemyHalfLength = 16.0f;
constexpr float EnemyHalfWidth = 3.0f;
constexpr float PlayerRadius = 12.0f;
constexpr Color4 BulletColor{255, 230, 120, 255};
constexpr Color4 EnemyColor{255, 110, 120, 255};
constexpr Color4 PlayerColor{120, 220, 255, 255};

InputBindings MakeBindings() {
    InputBindings bindings(1);
    bindings.Bind(0, RecipeShooterButton::Left, InputKey::A);
    bindings.Bind(0, RecipeShooterButton::Right, InputKey::D);
    bindings.Bind(0, RecipeShooterButton::Fire, InputKey::Space);
    return bindings;
}
} // namespace

RecipeShooterSimulation::RecipeShooterSimulation(const SimulationStart& start) {
    _player = _collision.Add(Shape::Circle(PlayerRadius), {{_playerX, PlayerY}}, MaskOf(RecipeLayer::Player));
    Rng rng(start.seed);
    (void)_collision.Add(Shape::Segment({0.0f, 0.0f}, {FieldWidth, 0.0f}), {}, MaskOf(RecipeLayer::Wall));
    for (int i = 0; i < EnemyCount; ++i) {
        const Pose pose{{rng.Float(0.0f, FieldWidth), rng.Float(100.0f, 300.0f)}, rng.Float(0.0f, 3.0f)};
        (void)RecipeEnemyColliders.Add(_collision, _enemies, {.pose = pose, .drift = rng.Float(-40.0f, 40.0f)},
                                       Shape::Box(EnemyHalfLength, EnemyHalfWidth));
    }
}
void RecipeShooterSimulation::Step(const SimulationStep& step) {
    const auto& input = step.Player(0);
    if (input.Held(RecipeShooterButton::Left)) _playerX -= PlayerSpeed * step.seconds;
    if (input.Held(RecipeShooterButton::Right)) _playerX += PlayerSpeed * step.seconds;
    _collision.Move(_player, {{_playerX, PlayerY}}); // The game owns the position; the world has to be told
    if (input.Pressed(RecipeShooterButton::Fire)) _bullets.push_back({_playerX, PlayerY});
    for (RecipeEnemy& enemy : _enemies) {
        enemy.pose.position.x += enemy.drift * step.seconds;
        if (enemy.pose.position.x < 0.0f || enemy.pose.position.x > FieldWidth) enemy.drift = -enemy.drift;
    }
    RecipeEnemyColliders.Sync(_collision, _enemies); // One line, no Move to forget

    const Shape bullet = Shape::Circle(BulletRadius);
    const Float2 move{0.0f, -BulletSpeed * step.seconds};
    std::erase_if(_bullets, [&](Float2& position) {
        const auto hit =
            _collision.Sweep(bullet, {position}, move, RecipeEnemyColliders.Layers() | MaskOf(RecipeLayer::Wall));
        if (!hit) {
            position += move;
            return false;
        }
        if (RecipeEnemy* enemy = _enemies.Find(RecipeEnemyColliders.Of(*hit))) enemy->hp -= 1; // Null for the wall
        return true;
    });
    _score += static_cast<int>(
        RecipeEnemyColliders.RemoveIf(_collision, _enemies, [](const RecipeEnemy& enemy) { return enemy.hp <= 0; }));
    _touches += static_cast<int>(_collision.Pairs(MaskOf(RecipeLayer::Player), MaskOf(RecipeLayer::Enemy)).size());
}
void RecipeShooterSimulation::State(StateVisitor& state) const {
    state.Field("playerX", _playerX);
    state.Field("score", _score);
    state.Field("touches", _touches);
    state.Field("bullet", _bullets);      // bullet[0].x, bullet[0].y, ...
    state.Field("enemies", _enemies);     // enemies[0].handle, enemies[0].hp, ..., and the pool's free slots
    state.Field("collision", _collision); // Dumps show colliders beside the game's positions
}
SimulationDefinition RecipeShooterDefinition() {
    SimulationDefinition definition;
    definition.settings.id = "myproject.shooter";
    definition.settings.buttons = {"Left", "Right", "Fire"};
    definition.create = [](const SimulationStart& start) { return std::make_unique<RecipeShooterSimulation>(start); };
    return definition;
}

void DrawRecipeShooter(Painter& paint, const RecipeShooterSimulation& simulation, bool showColliders) {
    paint.Circle({simulation.PlayerX(), PlayerY}, PlayerRadius, Fill(PlayerColor));
    for (const RecipeEnemy& enemy : simulation.Enemies()) {
        const Float2 half{std::cos(enemy.pose.angle) * EnemyHalfLength, std::sin(enemy.pose.angle) * EnemyHalfLength};
        paint.Line(enemy.pose.position - half, enemy.pose.position + half,
                   {.width = EnemyHalfWidth * 2.0f, .color = EnemyColor});
    }
    for (const Float2 bullet : simulation.Bullets())
        paint.Circle(bullet, BulletRadius, Fill(BulletColor));
    if (showColliders) DrawColliders(paint, simulation.Collision());
}

RecipeShooterScene::RecipeShooterScene(const SceneServices& services, std::uint64_t seed, bool showColliders)
    : _hosted(services.simulation, RecipeShooterDefinition().settings, SimulationStart{.seed = seed, .players = 1})
    , _bindings(MakeBindings())
    , _showColliders(showColliders) {}
void RecipeShooterScene::HandleInput(const DeviceInput& input) { _hosted.Runner().Feed(_bindings.Read(input)); }
void RecipeShooterScene::InputCancelled() { _hosted.Runner().CancelInput(); }
std::string RecipeShooterScene::ApplyParameter(const std::string& id, const SceneValue& value) {
    if (id == "showColliders") _showColliders = std::get<bool>(value);
    return {};
}
void RecipeShooterScene::Update(float seconds) { (void)_hosted.Runner().Advance(seconds); }
void RecipeShooterScene::Draw(Painter& paint) const { DrawRecipeShooter(paint, _hosted.Sim(), _showColliders); }
SceneDefinition RecipeShooterSceneDefinition() {
    SceneDefinition definition;
    definition.id = "myproject.shooter";
    definition.name = "Shooter";
    definition.category = "MyProject";
    definition.parameters = {
        {"seed", "Seed", SceneParameterKind::Integer, std::int64_t{1}, 0.0, 1000000.0, SceneApplyMode::OnReset},
        {"showColliders", "Show colliders", SceneParameterKind::Boolean, true}};
    definition.create = [](const SceneServices& services, const SceneParameters& parameters) {
        return std::make_unique<RecipeShooterScene>(
            services, static_cast<std::uint64_t>(std::get<std::int64_t>(parameters.at("seed"))),
            std::get<bool>(parameters.at("showColliders")));
    };
    return definition;
}
```
<!-- /recipe -->

Test it without a window: `python Engine sim run scenario.json --out build/run.replay`, with the scenario below. Comparing the
game's position with its collider's catches a forgotten `Move`, which hashes cannot.

```json
{ "simulation": "myproject.shooter", "seed": 4, "script": [
  { "expect": { "collision.colliders": 10, "enemies.count": 8 } },
  { "hold": "Right" }, { "run": 60 },
  { "expect": { "playerX": { "min": 639, "max": 641 },
                "collision.collider[0].position.x": { "min": 639, "max": 641 } } } ] }
```

See it: `python Engine ctl open myproject.shooter --set showColliders=true --screenshot --scale 0.5`. Debugging a hit that
goes wrong has its own procedure: [collision skill](../Skills/collision/SKILL.md).

Checks:

- A bullet is moved only after its sweep reports nothing, so it cannot jump over an enemy between steps.
- Every position the game changes goes through `Move`; the pool's enemies through one `RecipeEnemyColliders.Sync` per step. A forgotten `Move` replays the same way every time, so hashes will not catch it: compare the game's position with its collider in `python Engine sim state`, or draw with `showColliders` on.
- `RecipeEnemyColliders` is declared once beside `RecipeEnemy`; adding, syncing and removing go through it, so every enemy collider has the enemy's handle as owner and the Enemy layer. `Of(*hit)` gives the enemy's handle, or null for the wall. Dead enemies leave with their colliders in one `RemoveIf` per step, so two bullets hitting one enemy in a step cannot remove its collider twice.
- `sim state` lists each collider as `collision.collider[N].position.x`, `collision.collider[N].layers` and so on.
