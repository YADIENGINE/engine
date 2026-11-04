#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRegistry.h"
#include "Simulation/StepSmoothed.h"
#include "Utilities/Random/Rng.h"

enum class ToyButton { Left, Right, Fire };
enum class ToyAxis { Aim };

// A small two-player shooter's rules, used only to test the simulation system. Start parameters: "startScore", and
// "flaky", which reads static state at step 50 so that two runs in one process differ.
class ToySimulation final : public Simulation {
public:
    explicit ToySimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;

    [[nodiscard]] int Score() const { return _score; }
    [[nodiscard]] int Shots(std::size_t player) const { return _players[player].shots; }
    [[nodiscard]] int HeldSteps(std::size_t player) const { return _players[player].heldSteps; }
    [[nodiscard]] float X(std::size_t player) const { return _players[player].x.Current(); }
    [[nodiscard]] float SmoothedX(std::size_t player, float alpha) const { return _players[player].x.At(alpha); }
    [[nodiscard]] std::uint64_t Steps() const { return _steps; }

private:
    struct Player {
        StepSmoothed<float> x;
        float aim = 0.0f;
        int shots = 0;
        int heldSteps = 0;
    };
    std::array<Player, 2> _players;
    Rng _rng;
    float _enemyX = 0.0f;
    float _wave = 0.0f;
    int _score = 0;
    std::uint64_t _steps = 0;
    bool _flaky = false;
};

[[nodiscard]] SimulationDefinition ToyDefinition();
