#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "Modules/JsonIO/JsonIO.h"
#include "Simulation/PlayerInput.h"
#include "Simulation/SimulationSettings.h"
#include "Simulation/SimulationStep.h"
#include "Tuning/TuningLive.h"

// A recorded run: where it started and what each step received, as sparse changes. A change holds a player's whole
// input from that step on, except 'pressed', which applies to that step only. The file is JSON, readable and writable
// by an agent (Docs/Simulation.md).
class InputTape {
public:
    struct Change {
        std::uint64_t at = 0;
        int player = 0;
        PlayerInput input;
        friend bool operator==(const Change&, const Change&) = default;
    };
    struct TimedCommand {
        std::uint64_t at = 0;
        SimulationCommand command;
        friend bool operator==(const TimedCommand&, const TimedCommand&) = default;
    };

    InputTape() = default;
    InputTape(const SimulationSettings& settings, SimulationStart start);

    // Identity: where the tape came from. The constructor fills engineVersion and configuration.
    std::string simulation;
    int simulationVersion = 1;
    std::string engineVersion;
    std::string configuration; // Of the engine library that recorded it; information only
    int stepsPerSecond = 60;
    std::vector<std::string> buttons;
    std::vector<std::string> axes;
    SimulationStart start;

    std::uint64_t steps = 0;
    std::vector<Change> changes;        // In step order
    std::vector<TimedCommand> commands; // In step order
    // State hashes at steps 0, hashEvery, 2 * hashEvery ...; the state after that many steps have run.
    int hashEvery = 1;
    std::vector<std::uint64_t> hashes;
    // The tuning values the run started with (TuningDigests after its first recorded step), so a replay that disagrees
    // can name the files that changed since. Empty in Release, where values are compiled, and in older tapes.
    std::vector<TuningDigest> tuning;

    // Forgets step 'step' and everything after it; the state after 'step' steps stays known.
    void Truncate(std::uint64_t step);
    // Explains why this tape cannot play on 'settings'; empty when it can.
    [[nodiscard]] std::string Incompatibility(const SimulationSettings& settings) const;
    [[nodiscard]] bool SameVersions(const SimulationSettings& settings) const;

    [[nodiscard]] JsonIO::Document ToDocument() const;
    // Throws std::runtime_error (or JsonIO::Error) when the document is not a valid tape.
    [[nodiscard]] static InputTape FromDocument(const JsonIO::Document& document);
    void Save(const std::filesystem::path& file) const;
    [[nodiscard]] static InputTape Load(const std::filesystem::path& file);
    friend bool operator==(const InputTape&, const InputTape&) = default;
};
