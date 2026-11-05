#pragma once
#include <string_view>
#include <variant>
#include <vector>
#include "Simulation/Simulation.h"
#include "Simulation/SimulationEvents.h"
#include "Simulation/SimulationRegistry.h"
#include "Simulation/SimulationSettings.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Random/Rng.h"

enum class ArenaButton { Left, Right, Up, Down };

struct CoinTaken {
    static constexpr std::string_view Name = "CoinTaken";
    int score = 0;
    void State(StateVisitor& state) const;
};
struct PlayerHit {
    static constexpr std::string_view Name = "PlayerHit";
    int lives = 0;
    void State(StateVisitor& state) const;
};
struct GameOver {
    static constexpr std::string_view Name = "GameOver";
    int score = 0;
    void State(StateVisitor& state) const;
};
using ArenaEvent = std::variant<CoinTaken, PlayerHit, GameOver>;

// The arena's rules: the player collects coins while enemies chase them. Start parameters (all optional):
// "maxEnemies", "lives", "playerX"/"playerY", "coinX"/"coinY", "enemyX"/"enemyY" (one enemy at the start).
// Command "restart" starts a new run with the same start.
class ArenaSimulation final : public Simulation {
public:
    explicit ArenaSimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;
    [[nodiscard]] const SimulationEventLog* EventLog() const override { return &_events; }

    [[nodiscard]] Float2 Player() const { return _player; }
    [[nodiscard]] const std::vector<Float2>& Enemies() const { return _enemies; }
    [[nodiscard]] Float2 Coin() const { return _coin; }
    [[nodiscard]] int Score() const { return _score; }
    [[nodiscard]] int Lives() const { return _lives; }
    [[nodiscard]] bool Invulnerable() const { return _invulnerable > 0; }
    [[nodiscard]] bool Over() const { return _over; }
    [[nodiscard]] const SimulationEvents<ArenaEvent>& Events() const { return _events; }

private:
    void Reset();
    void MovePlayer(const SimulationStep& step);
    void MoveEnemies(const SimulationStep& step);
    void SpawnEnemy();
    void TakeCoin(const SimulationStep& step);
    void CheckHits(const SimulationStep& step);
    [[nodiscard]] Float2 RandomPoint(float margin);

    SimulationStart _start;
    Rng _rng;
    Float2 _player;
    std::vector<Float2> _enemies;
    Float2 _coin;
    int _maxEnemies = 0;
    int _spawnTimer = 0;
    int _score = 0;
    int _lives = 0;
    int _invulnerable = 0;
    bool _over = false;
    SimulationEvents<ArenaEvent> _events; // Output, not state
};

[[nodiscard]] const SimulationSettings& ArenaSettings();
[[nodiscard]] SimulationDefinition ArenaDefinition();
