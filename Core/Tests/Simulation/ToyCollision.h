#pragma once
#include <cstdint>
#include <vector>
#include "Physics/Collision/CollisionWorld.h"
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRegistry.h"
#include "Utilities/Random/Rng.h"

enum class ToyCollisionButton { Left, Right, Fire };

// A small shooter whose rules go through CollisionWorld, used only to test that collision replays exactly: turning
// enemies, a very fast bullet and a player who can touch them. Its float results feed the hash.
class ToyCollisionSimulation final : public Simulation {
public:
    explicit ToyCollisionSimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;

    [[nodiscard]] int Score() const { return _score; }
    [[nodiscard]] int WallHits() const { return _wallHits; }

private:
    struct Enemy {
        ColliderId collider;
        float angle = 0.0f;
        float spin = 0.0f;
        bool alive = true;
    };
    struct Bullet {
        Float2 position;
        bool dead = false;
    };
    Rng _rng;
    CollisionWorld _collision;
    std::vector<Enemy> _enemies;
    std::vector<Bullet> _bullets;
    ColliderId _player;
    float _playerX = 400.0f;
    int _score = 0;
    int _wallHits = 0;
    int _touches = 0;
    float _lastFraction = 0.0f;
};

[[nodiscard]] SimulationDefinition ToyCollisionDefinition();
