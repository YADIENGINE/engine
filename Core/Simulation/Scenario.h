#pragma once
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>
#include "Modules/JsonIO/JsonIO.h"
#include "Simulation/InputTape.h"
#include "Simulation/PlayerInput.h"
#include "Simulation/SimulationRegistry.h"
#include "Simulation/SimulationRunner.h"

// Plays a simulation without a window: give input, run steps, read state. C++ tests use this class directly; `<engine>
// sim run` drives it from a JSON script. Actions are named as the simulation declares them. Throws
// std::invalid_argument for an unknown name or player. Every step is hashed, so the tape can point at an exact step
// later.
class Scenario {
public:
    Scenario(const SimulationDefinition& definition, SimulationStart start);

    // Down from the next step until Release, pressed on the first of them.
    void Hold(std::string_view button, int player = 0);
    void Release(std::string_view button, int player = 0);
    // Pressed and held for the next step only.
    void Tap(std::string_view button, int player = 0);
    void SetAxis(std::string_view axis, float value, int player = 0);
    void Command(std::string_view id, std::int64_t value = 0);
    void Run(std::uint64_t steps = 1);

    [[nodiscard]] std::uint64_t Step() const { return _runner.StepIndex(); }
    [[nodiscard]] std::uint64_t Hash() const;
    // The flat state dump; the same names as Simulation::State.
    [[nodiscard]] JsonIO::Document State() const;
    [[nodiscard]] const Simulation& Sim() const { return *_simulation; }
    [[nodiscard]] const InputTape& Tape() const { return _runner.Tape(); }

private:
    [[nodiscard]] std::size_t ButtonIndex(std::string_view name) const;
    [[nodiscard]] PlayerInput& Player(int player);
    SimulationStart _start;
    std::unique_ptr<Simulation> _simulation;
    SimulationRunner _runner;
    std::vector<PlayerInput> _input;
    std::vector<std::uint32_t> _tapped;
};
