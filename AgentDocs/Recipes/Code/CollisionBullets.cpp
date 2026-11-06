// [recipe:CollisionBullets]
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
// [/recipe]
