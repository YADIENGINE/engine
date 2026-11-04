#include "Simulation/SimulationRunner.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include "Simulation/Simulation.h"
#include "Simulation/StateHash.h"

namespace {
constexpr double MaxOwedSeconds = 0.25;
// Frame durations arrive as floats; a sum this close to a whole number of steps is that whole number.
constexpr double WholeStepTolerance = 1e-4;

const SimulationSettings& Checked(const SimulationSettings& settings, const SimulationStart& start) {
    ValidateSimulation(settings);
    ValidateSimulationStart(settings, start);
    return settings;
}
} // namespace

SimulationRunner::SimulationRunner(Simulation& simulation, const SimulationSettings& settings,
                                   const SimulationStart& start)
    : _simulation(simulation)
    , _settings(Checked(settings, start))
    , _tape(settings, start)
    , _stepSeconds(1.0 / settings.stepsPerSecond)
    , _fixedSeconds(1.0f / static_cast<float>(settings.stepsPerSecond))
    , _latched(static_cast<std::size_t>(start.players))
    , _current(static_cast<std::size_t>(start.players)) {
    _tape.hashEvery = settings.stepsPerSecond;
}
void SimulationRunner::Play(InputTape tape, AfterTape after) {
    if (_step != 0 || _opened)
        throw std::invalid_argument("A tape plays from step 0; start a new simulation and runner");
    if (const auto problem = tape.Incompatibility(_settings); !problem.empty()) throw std::invalid_argument(problem);
    if (static_cast<std::size_t>(tape.start.players) != _latched.size())
        throw std::invalid_argument("The simulation was not started with the tape's players");
    _tape = std::move(tape);
    _after = after;
    _nextChange = 0;
    _nextCommand = 0;
}
void SimulationRunner::SetHashEvery(int steps) {
    if (steps < 1) throw std::invalid_argument("Hash interval must be at least 1");
    if (_opened) throw std::logic_error("Set the hash interval before the first step");
    _tape.hashEvery = steps;
    _tape.hashes.clear();
}
void SimulationRunner::Feed(std::span<const PlayerInput> players) {
    if (players.size() != _latched.size()) throw std::invalid_argument("Feed needs one input per player");
    if (_after == AfterTape::Stop) return; // A watched tape takes no input, during it or after it
    bool pressed = false;
    for (std::size_t i = 0; i < players.size(); ++i) {
        _latched[i].pressed |= players[i].pressed;
        _latched[i].held = players[i].held;
        _latched[i].axes = players[i].axes;
        pressed = pressed || players[i].pressed != 0U;
    }
    if (pressed && Replaying()) _tape.Truncate(_step);
}
void SimulationRunner::CancelInput() {
    for (auto& player : _latched)
        player = PlayerInput{};
}
void SimulationRunner::Command(std::string id, std::int64_t value) {
    if (_after == AfterTape::Stop) return;
    if (Replaying()) _tape.Truncate(_step);
    _pending.push_back({std::move(id), value});
}
int SimulationRunner::Advance(double seconds) {
    if (!(seconds > 0.0)) return 0;
    _accumulator += seconds;
    double owed = _accumulator / _stepSeconds;
    const double whole = std::round(owed);
    const bool exact = std::fabs(owed - whole) < WholeStepTolerance;
    if (exact) owed = whole;
    auto count = static_cast<std::int64_t>(std::floor(owed));
    const auto limit = static_cast<std::int64_t>(std::ceil(MaxOwedSeconds * _settings.stepsPerSecond));
    if (count > limit) {
        count = limit;
        _accumulator = 0.0;
    } else if (exact) {
        _accumulator = 0.0;
    } else {
        _accumulator = std::max(0.0, _accumulator - static_cast<double>(count) * _stepSeconds);
    }
    int ran = 0;
    for (std::int64_t i = 0; i < count && RunStep(); ++i)
        ++ran;
    return ran;
}
void SimulationRunner::Checkpoint() {
    const auto every = static_cast<std::uint64_t>(_tape.hashEvery);
    if (_step % every != 0U) return;
    const auto index = static_cast<std::size_t>(_step / every);
    const std::uint64_t hash = HashState(_simulation);
    if (index >= _tape.hashes.size()) {
        _tape.hashes.push_back(hash);
    } else if (_mismatch) {
        return;
    } else if (_tape.hashes[index] == hash) {
        _lastMatch = _step;
    } else {
        _mismatch = TapeMismatch{_lastMatch, _step};
    }
}
bool SimulationRunner::RunStep() {
    if (!_opened) {
        _opened = true;
        Checkpoint();
    }
    if (Finished()) return false;
    _stepCommands.clear();
    if (_step < _tape.steps) {
        for (auto& player : _current)
            player.pressed = 0U;
        while (_nextChange < _tape.changes.size() && _tape.changes[_nextChange].at == _step) {
            const auto& change = _tape.changes[_nextChange++];
            _current[static_cast<std::size_t>(change.player)] = change.input;
        }
        while (_nextCommand < _tape.commands.size() && _tape.commands[_nextCommand].at == _step)
            _stepCommands.push_back(_tape.commands[_nextCommand++].command);
    } else {
        for (std::size_t i = 0; i < _latched.size(); ++i) {
            const PlayerInput input = _latched[i];
            _latched[i].pressed = 0U; // A press reaches one step only
            const bool same = input.held == _current[i].held && input.axes == _current[i].axes;
            if (!same || input.pressed != 0U) _tape.changes.push_back({_step, static_cast<int>(i), input});
            _current[i] = input;
        }
        _stepCommands.swap(_pending);
        for (const auto& command : _stepCommands)
            _tape.commands.push_back({_step, command});
        _tape.steps = _step + 1U;
        _nextChange = _tape.changes.size();
        _nextCommand = _tape.commands.size();
    }
    const bool firstRecorded = _step == 0U && _step + 1U == _tape.steps;
    _simulation.Step({_step, _fixedSeconds, _current, _stepCommands});
    if (firstRecorded) _tape.tuning = TuningDigests(); // The simulation has read its tuning by now
    ++_step;
    Checkpoint();
    return true;
}
void SimulationRunner::RunTo(std::uint64_t step) {
    while (_step < step && RunStep()) {
    }
}
float SimulationRunner::Alpha() const { return static_cast<float>(std::clamp(_accumulator / _stepSeconds, 0.0, 1.0)); }
