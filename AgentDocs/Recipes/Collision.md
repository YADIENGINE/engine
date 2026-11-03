# Collision recipe

Stop fast bullets, find overlapping pairs and show colliders on screen, inside a simulation that replays exactly.

APIs: Core/Physics/Collision/CollisionWorld.h, Core/Physics/Collision/Shape.h, Core/Physics/Collision/CollisionMask.h, Core/Physics/Debug/ColliderDraw.h. Concepts: [Collision](../../Docs/Collision.md).

Put the rules in Game/Simulation/: `engine_add_simulation` already links collision, so including the headers is enough. Keep the drawing function outside that folder.

<!-- recipe:CollisionBullets-header -->
```cpp
#pragma once
#include <vector>
#include "Physics/Collision/CollisionWorld.h"
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRegistry.h"

class Painter;

enum class RecipeLayer { Player, Enemy, Wall };
enum class RecipeShooterButton { Left, Right, Fire };

// The rules, in Game/Simulation: colliders are found through the world, positions stay in the game's own fields.
class RecipeShooterSimulation final : public Simulation {
public:
    explicit RecipeShooterSimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;

    [[nodiscard]] int Score() const { return _score; }
    [[nodiscard]] int Touches() const { return _touches; }
    [[nodiscard]] const CollisionWorld& Collision() const { return _collision; }
    [[nodiscard]] const std::vector<Float2>& Bullets() const { return _bullets; }

private:
    CollisionWorld _collision;
    ColliderId _player;
    std::vector<Float2> _bullets; // Bullets only ask questions, so they need no collider of their own
    float _playerX = 400.0f;
    int _score = 0;
    int _touches = 0;
};

[[nodiscard]] SimulationDefinition RecipeShooterDefinition();
// The view, outside Game/Simulation. A debug switch shows what the rules see on top of what the player sees.
void DrawRecipeShooter(Painter& paint, const RecipeShooterSimulation& simulation, bool showColliders);
```
<!-- /recipe -->

<!-- recipe:CollisionBullets -->
```cpp
#include "AgentDocs/Recipes/Code/CollisionBullets.h"
#include <algorithm>
#include <limits>
#include <memory>
#include "Graphics/Paint/PaintStyle.h"
#include "Graphics/Paint/Painter.h"
#include "Physics/Debug/ColliderDraw.h"
#include "Simulation/StateVisitor.h"
#include "Utilities/Rng.h"

namespace {
constexpr std::uint64_t WallOwner = std::numeric_limits<std::uint64_t>::max();
constexpr float FieldWidth = 800.0f;
constexpr float PlayerY = 500.0f;
constexpr float PlayerSpeed = 240.0f;
constexpr float BulletSpeed = 3000.0f; // 50 units per step: without a sweep it would jump over a thin enemy
constexpr float BulletRadius = 4.0f;
constexpr int EnemyCount = 8;
constexpr Color4 BulletColor{255, 230, 120, 255};
} // namespace

RecipeShooterSimulation::RecipeShooterSimulation(const SimulationStart& start) {
    _player = _collision.Add(Shape::Circle(12.0f), {{_playerX, PlayerY}}, MaskOf(RecipeLayer::Player));
    Rng rng(start.seed);
    (void)_collision.Add(Shape::Segment({0.0f, 0.0f}, {FieldWidth, 0.0f}), {}, MaskOf(RecipeLayer::Wall), WallOwner);
    for (int i = 0; i < EnemyCount; ++i) {
        const Pose pose{{rng.Float(0.0f, FieldWidth), rng.Float(100.0f, 300.0f)}, rng.Float(0.0f, 3.0f)};
        (void)_collision.Add(Shape::Box(16.0f, 3.0f), pose, MaskOf(RecipeLayer::Enemy));
    }
}
void RecipeShooterSimulation::Step(const SimulationStep& step) {
    const auto& input = step.Player(0);
    if (input.Held(RecipeShooterButton::Left)) _playerX -= PlayerSpeed * step.seconds;
    if (input.Held(RecipeShooterButton::Right)) _playerX += PlayerSpeed * step.seconds;
    _collision.Move(_player, {{_playerX, PlayerY}}); // The game owns the position; the world has to be told
    if (input.Pressed(RecipeShooterButton::Fire)) _bullets.push_back({_playerX, PlayerY});

    const Shape bullet = Shape::Circle(BulletRadius);
    const Float2 move{0.0f, -BulletSpeed * step.seconds};
    std::erase_if(_bullets, [&](Float2& position) {
        const auto hit = _collision.Sweep(bullet, {position}, move, MaskOf(RecipeLayer::Enemy, RecipeLayer::Wall));
        if (!hit) {
            position += move;
            return false;
        }
        if (hit->owner != WallOwner) {
            _collision.Remove(hit->id);
            ++_score;
        }
        return true;
    });
    _touches += static_cast<int>(_collision.Pairs(MaskOf(RecipeLayer::Player), MaskOf(RecipeLayer::Enemy)).size());
}
void RecipeShooterSimulation::State(StateVisitor& state) const {
    state.Field("playerX", _playerX);
    state.Field("score", _score);
    state.Field("touches", _touches);
    for (std::size_t i = 0; i < _bullets.size(); ++i)
        state.Group("bullet", i, [&] { state.Field("position", _bullets[i]); });
    state.Group("collision", [&] { _collision.State(state); }); // Dumps show colliders beside the game's positions
}
SimulationDefinition RecipeShooterDefinition() {
    SimulationDefinition definition;
    definition.settings.id = "myproject.shooter";
    definition.settings.buttons = {"Left", "Right", "Fire"};
    definition.create = [](const SimulationStart& start) { return std::make_unique<RecipeShooterSimulation>(start); };
    return definition;
}

void DrawRecipeShooter(Painter& paint, const RecipeShooterSimulation& simulation, bool showColliders) {
    for (const Float2 bullet : simulation.Bullets())
        paint.Circle(bullet, BulletRadius, Fill(BulletColor));
    if (showColliders) DrawColliders(paint, simulation.Collision());
}
```
<!-- /recipe -->

Checks:

- A bullet is moved only after its sweep reports nothing, so it cannot jump over an enemy between steps.
- Every position the game changes goes through `Move`. A forgotten `Move` replays the same way every time, so hashes will not catch it: compare the game's position with its collider in `python Engine sim state`, or draw with `showColliders` on.
- `owner` connects a found collider to the game's own data; here it marks the wall.
- `sim state` lists each collider as `collision.collider[N].position.x`, `collision.collider[N].layers` and so on.
