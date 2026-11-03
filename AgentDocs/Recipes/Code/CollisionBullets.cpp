// [recipe:CollisionBullets]
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
// [/recipe]
