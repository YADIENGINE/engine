#include "Simulation/Scenario.h"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include "Simulation/StateDump.h"
#include "Simulation/StateHash.h"

namespace {
std::unique_ptr<Simulation> Create(const SimulationDefinition& definition, const SimulationStart& start) {
    ValidateSimulationStart(definition.settings, start);
    auto simulation = definition.create(start);
    if (!simulation) throw std::runtime_error("The simulation factory returned nothing");
    return simulation;
}
} // namespace

Scenario::Scenario(const SimulationDefinition& definition, SimulationStart start)
    : _start(std::move(start))
    , _simulation(Create(definition, _start))
    , _runner(*_simulation, definition.settings, _start)
    , _input(static_cast<std::size_t>(_start.players))
    , _tapped(static_cast<std::size_t>(_start.players)) {
    _runner.SetHashEvery(1);
}
std::size_t Scenario::ButtonIndex(std::string_view name) const {
    const auto& buttons = _runner.Settings().buttons;
    const auto found = std::ranges::find(buttons, name);
    if (found == buttons.end()) throw std::invalid_argument("Unknown button: " + std::string(name));
    return static_cast<std::size_t>(found - buttons.begin());
}
PlayerInput& Scenario::Player(int player) {
    if (player < 0 || player >= _start.players)
        throw std::invalid_argument("No such player: " + std::to_string(player));
    return _input[static_cast<std::size_t>(player)];
}
void Scenario::Hold(std::string_view button, int player) {
    auto& input = Player(player);
    const auto index = ButtonIndex(button);
    if (!input.Held(index)) input.SetPressed(index);
    input.SetHeld(index, true);
}
void Scenario::Release(std::string_view button, int player) { Player(player).SetHeld(ButtonIndex(button), false); }
void Scenario::Tap(std::string_view button, int player) {
    auto& input = Player(player);
    const auto index = ButtonIndex(button);
    input.SetPressed(index);
    input.SetHeld(index, true);
    _tapped[static_cast<std::size_t>(player)] |= std::uint32_t{1} << index;
}
void Scenario::SetAxis(std::string_view axis, float value, int player) {
    const auto& axes = _runner.Settings().axes;
    const auto found = std::ranges::find(axes, axis);
    if (found == axes.end()) throw std::invalid_argument("Unknown axis: " + std::string(axis));
    Player(player).SetAxis(static_cast<std::size_t>(found - axes.begin()), value);
}
void Scenario::Command(std::string_view id, std::int64_t value) { _runner.Command(std::string(id), value); }
void Scenario::Run(std::uint64_t steps) {
    for (std::uint64_t i = 0; i < steps; ++i) {
        _runner.Feed(_input);
        (void)_runner.RunStep();
        for (std::size_t player = 0; player < _input.size(); ++player) {
            _input[player].pressed = 0U;
            _input[player].held &= ~_tapped[player];
            _tapped[player] = 0U;
        }
    }
}
std::uint64_t Scenario::Hash() const { return HashState(*_simulation); }
JsonIO::Document Scenario::State() const { return DumpState(*_simulation); }
