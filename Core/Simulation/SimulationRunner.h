#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "Simulation/InputTape.h"
#include "Simulation/PlayerInput.h"
#include "Simulation/SimulationSettings.h"
#include "Simulation/SimulationStep.h"

class Simulation;

// What happens when a played tape ends: Stop for a replay a player watches; Live to take over input, and a press or a
// command during the tape then cuts it at the current step.
enum class AfterTape { Stop, Live };

// Where a played tape first disagreed with the running state: the first differing step lies in
// (lastMatch, firstMismatch]. Without a lastMatch even the start differs.
struct TapeMismatch {
    std::optional<std::uint64_t> lastMatch;
    std::uint64_t firstMismatch = 0;
};

// Runs a simulation in fixed steps between variable frames. It owns the frame-to-step bridge: feed input every frame,
// advance by scaled seconds (TimeScale already applied), draw with Alpha. Its position is always within one tape: steps
// inside the tape replay it, steps past its end are live and extend it. Going back means a new simulation and runner
// with Play. A paused scene neither feeds nor advances.
class SimulationRunner {
public:
    SimulationRunner(Simulation& simulation, const SimulationSettings& settings, const SimulationStart& start);

    // A tape to replay from step 0. Throws std::invalid_argument if it cannot play on this simulation or if steps ran.
    void Play(InputTape tape, AfterTape after);
    // Hashes for the live part of the tape; call before the first step. Default: one second of steps.
    void SetHashEvery(int steps);

    // Input for the next step, one entry per player. Presses wait for the next step and reach only that one, even if
    // the next frame owes no step or several; held state and axes always follow the latest frame.
    void Feed(std::span<const PlayerInput> players);
    // Focus loss or scene departure: pending presses are dropped and held actions released.
    void CancelInput();
    // Delivered with the next step and recorded with it.
    void Command(std::string id, std::int64_t value = 0);

    // Runs the steps these seconds owe and returns how many ran. Time beyond a quarter of a second in one frame is
    // dropped; steps are what the tape counts, so this cannot harm a replay.
    int Advance(double seconds);
    // Forgets time owed towards the next step: fixed stepping lands on whole steps, whatever real time came before.
    void DropRemainder() { _accumulator = 0.0; }
    // Runs exactly one step; false when a Stop tape has ended.
    bool RunStep();
    // Runs steps until StepIndex() reaches 'step' or a Stop tape ends; never goes back. This is catching up: the
    // events of these steps are silenced (SilenceEventsUntil).
    void RunTo(std::uint64_t step);
    // Steps before 'step' are catching up, not played for the player: SimulationEventReader::Read skips their events,
    // so a seek does not play the sounds of every step it passes. The editor sets it while it seeks.
    void SilenceEventsUntil(std::uint64_t step) { _silencedUntil = step; }
    [[nodiscard]] std::uint64_t EventsSilencedUntil() const { return _silencedUntil; }

    // How far the frame is between the last step and the next, 0..1, for drawing.
    [[nodiscard]] float Alpha() const;
    // Steps run so far; also the index of the next step.
    [[nodiscard]] std::uint64_t StepIndex() const { return _step; }
    [[nodiscard]] const SimulationSettings& Settings() const { return _settings; }
    [[nodiscard]] bool Replaying() const { return _step < _tape.steps; }
    [[nodiscard]] bool Finished() const { return _after == AfterTape::Stop && _step >= _tape.steps; }
    [[nodiscard]] const InputTape& Tape() const { return _tape; }
    // Hands the tape over, for a new runner after a rewind; this runner must not run any more.
    [[nodiscard]] InputTape TakeTape() { return std::move(_tape); }
    [[nodiscard]] const std::optional<TapeMismatch>& Mismatch() const { return _mismatch; }
    [[nodiscard]] const Simulation& Sim() const { return _simulation; }

private:
    void Checkpoint();
    Simulation& _simulation;
    SimulationSettings _settings;
    InputTape _tape;
    AfterTape _after = AfterTape::Live;
    double _stepSeconds;
    float _fixedSeconds;
    double _accumulator = 0.0;
    std::uint64_t _step = 0;
    std::uint64_t _silencedUntil = 0;
    std::size_t _nextChange = 0, _nextCommand = 0;
    bool _opened = false;
    std::vector<PlayerInput> _latched; // Latest frame's input, with presses gathered since the last step
    std::vector<PlayerInput> _current; // What the last step received
    std::vector<SimulationCommand> _pending;
    std::vector<SimulationCommand> _stepCommands;
    std::optional<TapeMismatch> _mismatch;
    std::optional<std::uint64_t> _lastMatch;

public:
    SimulationRunner(const SimulationRunner&) = delete;
    SimulationRunner& operator=(const SimulationRunner&) = delete;
};
