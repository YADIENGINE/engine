#include "Tests/Simulation/ToySimulation.h"
#include <cmath>
#include <cstddef>
#include <memory>
#include "Simulation/StateVisitor.h"

namespace {
constexpr float MoveSpeed = 120.0f;
constexpr float FieldWidth = 400.0f;
constexpr float HitRange = 60.0f;
constexpr int HitScore = 10;
constexpr std::uint64_t FlakyStep = 50;

int ReadStartScore(const SimulationStart& start) {
    const auto& members = start.parameters.Members();
    const auto found = members.find("startScore");
    return found == members.end() ? 0 : static_cast<int>(found->second.AsInt64());
}
bool ReadFlaky(const SimulationStart& start) {
    const auto& members = start.parameters.Members();
    const auto found = members.find("flaky");
    return found != members.end() && found->second.AsBool();
}
int NextFlakyValue() {
    static int calls = 0; // Deliberately outside the state
    return ++calls;
}
} // namespace

ToySimulation::ToySimulation(const SimulationStart& start)
    : _rng(start.seed)
    , _enemyX(_rng.Float(0.0f, FieldWidth))
    , _score(ReadStartScore(start))
    , _flaky(ReadFlaky(start)) {
    _players[1].x.Snap(FieldWidth);
}
void ToySimulation::Step(const SimulationStep& step) {
    for (std::size_t i = 0; i < step.players.size(); ++i) {
        const auto& input = step.Player(i);
        auto& player = _players[i];
        float direction = 0.0f;
        if (input.Held(ToyButton::Left)) direction -= 1.0f;
        if (input.Held(ToyButton::Right)) direction += 1.0f;
        if (input.Held(ToyButton::Fire)) ++player.heldSteps;
        player.x.Set(player.x.Current() + direction * MoveSpeed * step.seconds);
        player.aim = input.AxisValue(ToyAxis::Aim);
        if (input.Pressed(ToyButton::Fire)) {
            ++player.shots;
            if (std::fabs(player.x.Current() + player.aim - _enemyX) < HitRange) {
                _score += HitScore;
                _enemyX = _rng.Float(0.0f, FieldWidth);
            }
        }
    }
    for (const auto& command : step.commands)
        if (command.id == "bonus") _score += static_cast<int>(command.value);
    _wave += std::sin(static_cast<float>(step.index) * 0.1f) * step.seconds;
    if (_flaky && step.index == FlakyStep) _score += NextFlakyValue();
    ++_steps;
}
void ToySimulation::State(StateVisitor& state) const {
    state.Field("score", _score);
    state.Field("steps", _steps);
    state.Field("enemyX", _enemyX);
    state.Field("wave", _wave);
    state.Field("rng", _rng);
    for (std::size_t i = 0; i < _players.size(); ++i) {
        state.Group("player", i, [&] {
            state.Field("x", _players[i].x.Current());
            state.Field("aim", _players[i].aim);
            state.Field("shots", _players[i].shots);
            state.Field("heldSteps", _players[i].heldSteps);
        });
    }
}

SimulationDefinition ToyDefinition() {
    SimulationDefinition definition;
    definition.settings.id = "engine.toy";
    definition.settings.maxPlayers = 2;
    definition.settings.buttons = {"Left", "Right", "Fire"};
    definition.settings.axes = {"Aim"};
    definition.create = [](const SimulationStart& start) { return std::make_unique<ToySimulation>(start); };
    return definition;
}
void RegisterSimulations(SimulationRegistry& registry) { registry.Add(ToyDefinition()); }
