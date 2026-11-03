#include "Tests/Simulation/ToyCollision.h"
#include <algorithm>
#include <limits>
#include <memory>
#include "Simulation/StateVisitor.h"

namespace {
enum class Layer { Player, Enemy, Wall };
constexpr std::uint64_t WallOwner = std::numeric_limits<std::uint64_t>::max();
constexpr float PlayerY = 500.0f;
constexpr float PlayerSpeed = 240.0f;
constexpr float BulletSpeed = 6000.0f; // 100 units per step: far more than an enemy is wide
constexpr float FieldWidth = 800.0f;
constexpr int EnemyCount = 11;
} // namespace

ToyCollisionSimulation::ToyCollisionSimulation(const SimulationStart& start)
    : _rng(start.seed) {
    _player = _collision.Add(Shape::Circle(10.0f), {{_playerX, PlayerY}}, MaskOf(Layer::Player));
    (void)_collision.Add(Shape::Segment({0.0f, 0.0f}, {FieldWidth, 0.0f}), {}, MaskOf(Layer::Wall), WallOwner);
    for (int i = 0; i < EnemyCount; ++i) {
        Enemy enemy;
        enemy.spin = _rng.Float(-3.0f, 3.0f);
        enemy.collider = _collision.Add(Shape::Box(14.0f, 3.0f), {{FieldWidth * static_cast<float>(i) / 10.0f, 200.0f}},
                                        MaskOf(Layer::Enemy), static_cast<std::uint64_t>(i));
        _enemies.push_back(enemy);
    }
}
void ToyCollisionSimulation::Step(const SimulationStep& step) {
    const auto& input = step.Player(0);
    if (input.Held(ToyCollisionButton::Left)) _playerX -= PlayerSpeed * step.seconds;
    if (input.Held(ToyCollisionButton::Right)) _playerX += PlayerSpeed * step.seconds;
    _collision.Move(_player, {{_playerX, PlayerY}});
    if (input.Pressed(ToyCollisionButton::Fire)) _bullets.push_back({{_playerX, PlayerY}});

    for (auto& enemy : _enemies) {
        if (!enemy.alive) continue;
        enemy.angle += enemy.spin * step.seconds;
        const Pose pose = _collision.PoseOf(enemy.collider);
        _collision.Move(enemy.collider, {pose.position, enemy.angle});
    }
    const Shape bulletShape = Shape::Circle(2.0f);
    for (auto& bullet : _bullets) {
        const Float2 move{0.0f, -BulletSpeed * step.seconds};
        const auto hit = _collision.Sweep(bulletShape, {bullet.position}, move, MaskOf(Layer::Enemy, Layer::Wall));
        if (!hit) {
            bullet.position += move;
            continue;
        }
        bullet.dead = true;
        _lastFraction = hit->fraction;
        if (hit->owner == WallOwner) {
            ++_wallHits;
        } else {
            ++_score;
            _enemies[static_cast<std::size_t>(hit->owner)].alive = false;
            _collision.Remove(hit->id);
        }
    }
    std::erase_if(_bullets, [](const Bullet& bullet) { return bullet.dead; });
    _touches += static_cast<int>(_collision.Pairs(MaskOf(Layer::Player), MaskOf(Layer::Enemy)).size());
}
void ToyCollisionSimulation::State(StateVisitor& state) const {
    state.Field("score", _score);
    state.Field("wallHits", _wallHits);
    state.Field("touches", _touches);
    state.Field("lastFraction", _lastFraction);
    state.Field("playerX", _playerX);
    state.Field("rng", _rng);
    for (std::size_t i = 0; i < _bullets.size(); ++i)
        state.Group("bullet", i, [&] { state.Field("position", _bullets[i].position); });
    for (std::size_t i = 0; i < _enemies.size(); ++i) {
        state.Group("enemy", i, [&] {
            state.Field("alive", _enemies[i].alive);
            state.Field("angle", _enemies[i].angle);
        });
    }
    state.Group("collision", [&] { _collision.State(state); });
}

SimulationDefinition ToyCollisionDefinition() {
    SimulationDefinition definition;
    definition.settings.id = "engine.toy-collision";
    definition.settings.buttons = {"Left", "Right", "Fire"};
    definition.create = [](const SimulationStart& start) { return std::make_unique<ToyCollisionSimulation>(start); };
    return definition;
}
