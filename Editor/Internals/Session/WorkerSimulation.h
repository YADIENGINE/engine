#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <optional>
#include <string>
#include "Editor/Api/SimulationHost.h"
#include "Simulation/InputTape.h"

// The worker's record of the open scene's simulation: one tape, kept while the scene is rebuilt. In a scene with a
// simulation this replaces the action timeline (D26): input and scene actions are both on the tape.
class WorkerSimulation final : public SimulationHost {
public:
    [[nodiscard]] SimulationStart StartFor(const SimulationSettings& settings, SimulationStart fallback) override;
    void Attach(SimulationRunner& runner) override;
    void Detach(SimulationRunner& runner) override;

    [[nodiscard]] SimulationRunner* Runner() const { return _runner; }
    // Seconds of one step, for fixed playback; 'fallback' without a simulation.
    [[nodiscard]] double StepSeconds(double fallback) const;
    // Before the scene is rebuilt to go back: the tape waits for the next runner.
    void PrepareRewind();
    // Open or reset: the next run records from nothing.
    void Forget();
    // Stages a replay for the next runner; false with LastError set when it cannot play on the open simulation.
    [[nodiscard]] bool Load(const std::filesystem::path& file);
    [[nodiscard]] bool Save(const std::filesystem::path& file);
    void SetLastError(std::string text) { _lastError = std::move(text); }
    // {"step", "stepsPerSecond", "tapeSteps", "replay", optional "moments", "mismatch" and "error"}; null without a simulation.
    [[nodiscard]] nlohmann::json State() const;

private:
    SimulationRunner* _runner = nullptr;
    std::optional<InputTape> _tape;
    std::optional<SimulationStart> _loadedStart; // A loaded replay keeps its own start and hashes across rewinds
    std::string _lastError;
};
