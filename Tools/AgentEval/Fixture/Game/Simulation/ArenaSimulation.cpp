#include "Simulation/ArenaSimulation.h"
#include <algorithm>
#include <memory>
#include "Simulation/ArenaRules.h"
#include "Simulation/StateVisitor.h"

namespace {
constexpr int SpawnTries = 8;

float Parameter(const SimulationStart& start, const char* name, float fallback) {
    const auto& members = start.parameters.Members();
    const auto found = members.find(name);
    return found == members.end() ? fallback : static_cast<float>(found->second.AsInt64());
}
bool HasParameter(const SimulationStart& start, const char* name) {
    return start.parameters.Members().contains(name);
}
} // namespace

void CoinTaken::State(StateVisitor& state) const { state.Field("score", score); }
void PlayerHit::State(StateVisitor& state) const { state.Field("lives", lives); }
void GameOver::State(StateVisitor& state) const { state.Field("score", score); }

ArenaSimulation::ArenaSimulation(const SimulationStart& start)
    : _start(start) {
    Reset();
}

void ArenaSimulation::Reset() {
    _rng = Rng(_start.seed);
    _player = {Parameter(_start, "playerX", (ArenaLeft + ArenaRight) * 0.5f),
               Parameter(_start, "playerY", (ArenaTop + ArenaBottom) * 0.5f)};
    _maxEnemies = static_cast<int>(Parameter(_start, "maxEnemies", static_cast<float>(MaxEnemies)));
    _enemies.clear();
    if (HasParameter(_start, "enemyX"))
        _enemies.push_back({Parameter(_start, "enemyX", 0.0f), Parameter(_start, "enemyY", 0.0f)});
    _coin = HasParameter(_start, "coinX") ? Float2{Parameter(_start, "coinX", 0.0f), Parameter(_start, "coinY", 0.0f)}
                                          : RandomPoint(CoinRadius * 4.0f);
    _spawnTimer = SpawnEverySteps;
    _score = 0;
    _lives = static_cast<int>(Parameter(_start, "lives", static_cast<float>(StartLives)));
    _invulnerable = 0;
    _over = false;
}

void ArenaSimulation::Step(const SimulationStep& step) {
    for (const auto& command : step.commands)
        if (command.id == "restart") Reset();
    if (_over) return;
    MovePlayer(step);
    MoveEnemies(step);
    TakeCoin(step);
    CheckHits(step);
}

void ArenaSimulation::MovePlayer(const SimulationStep& step) {
    const auto& input = step.Player(0);
    Float2 direction;
    if (input.Held(ArenaButton::Left)) direction.x -= 1.0f;
    if (input.Held(ArenaButton::Right)) direction.x += 1.0f;
    if (input.Held(ArenaButton::Up)) direction.y -= 1.0f;
    if (input.Held(ArenaButton::Down)) direction.y += 1.0f;
    _player += Normalized(direction) * (PlayerSpeed * step.seconds);
    _player.x = std::clamp(_player.x, ArenaLeft + PlayerRadius, ArenaRight - PlayerRadius);
    _player.y = std::clamp(_player.y, ArenaTop + PlayerRadius, ArenaBottom - PlayerRadius);
}

void ArenaSimulation::MoveEnemies(const SimulationStep& step) {
    for (auto& enemy : _enemies)
        enemy += Normalized(_player - enemy) * (EnemySpeed * step.seconds);
    if (static_cast<int>(_enemies.size()) >= _maxEnemies) return;
    if (--_spawnTimer > 0) return;
    _spawnTimer = SpawnEverySteps;
    SpawnEnemy();
}

void ArenaSimulation::SpawnEnemy() {
    Float2 point = RandomPoint(EnemyRadius);
    for (int attempt = 1; attempt < SpawnTries && Distance(point, _player) < SpawnMinDistance; ++attempt)
        point = RandomPoint(EnemyRadius);
    _enemies.push_back(point);
}

void ArenaSimulation::TakeCoin(const SimulationStep& step) {
    if (Distance(_player, _coin) > PlayerRadius + CoinRadius) return;
    _score += CoinScore;
    _events.Emit(step, CoinTaken{_score});
    _coin = RandomPoint(CoinRadius * 4.0f);
}

void ArenaSimulation::CheckHits(const SimulationStep& step) {
    if (_invulnerable > 0) {
        --_invulnerable;
        return;
    }
    const bool hit = std::ranges::any_of(
        _enemies, [&](Float2 enemy) { return Distance(enemy, _player) < PlayerRadius + EnemyRadius; });
    if (!hit) return;
    --_lives;
    _invulnerable = InvulnerableSteps;
    _events.Emit(step, PlayerHit{_lives});
    if (_lives > 0) return;
    _over = true;
    _events.Emit(step, GameOver{_score});
}

Float2 ArenaSimulation::RandomPoint(float margin) {
    const float x = _rng.Float(ArenaLeft + margin, ArenaRight - margin);
    const float y = _rng.Float(ArenaTop + margin, ArenaBottom - margin);
    return {x, y};
}

void ArenaSimulation::State(StateVisitor& state) const {
    state.Field("rng", _rng);
    state.Group("player", [&] {
        state.Field("x", _player.x);
        state.Field("y", _player.y);
    });
    state.Field("enemies", static_cast<int>(_enemies.size()));
    for (std::size_t i = 0; i < _enemies.size(); ++i)
        state.Group("enemy", i, [&] {
            state.Field("x", _enemies[i].x);
            state.Field("y", _enemies[i].y);
        });
    state.Group("coin", [&] {
        state.Field("x", _coin.x);
        state.Field("y", _coin.y);
    });
    state.Field("maxEnemies", _maxEnemies);
    state.Field("spawnTimer", _spawnTimer);
    state.Field("score", _score);
    state.Field("lives", _lives);
    state.Field("invulnerable", _invulnerable);
    state.Field("over", _over);
}

const SimulationSettings& ArenaSettings() {
    static const SimulationSettings settings = [] {
        SimulationSettings made;
        made.id = "arena.play";
        made.buttons = {"Left", "Right", "Up", "Down"};
        return made;
    }();
    return settings;
}

SimulationDefinition ArenaDefinition() {
    return {ArenaSettings(), [](const SimulationStart& start) { return std::make_unique<ArenaSimulation>(start); }};
}

void RegisterSimulations(SimulationRegistry& registry) { registry.Add(ArenaDefinition()); }
