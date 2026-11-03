#include "Editor/Internals/Session/WorkerSimulation.h"
#include <exception>
#include <utility>

SimulationStart WorkerSimulation::StartFor(const SimulationSettings& settings, SimulationStart fallback) {
    if (_loadedStart && _loadedStart->players <= settings.maxPlayers) return *_loadedStart;
    return fallback;
}
void WorkerSimulation::Attach(SimulationRunner& runner) {
    _runner = &runner;
    if (_tape) {
        auto tape = std::move(*_tape);
        _tape.reset();
        const auto& start = runner.Tape().start;
        // A changed player count leaves nothing the old input could mean; the run starts over.
        if (tape.start.players == start.players && tape.Incompatibility(runner.Settings()).empty()) {
            if (!_loadedStart) { // Rewinding uses current parameters, so old hashes say nothing
                tape.start = start;
                tape.hashes.clear();
            }
            runner.Play(std::move(tape), AfterTape::Live);
            return;
        }
    }
    runner.SetHashEvery(1);
}
void WorkerSimulation::Detach(SimulationRunner& runner) {
    if (_runner == &runner) _runner = nullptr;
}
double WorkerSimulation::StepSeconds(double fallback) const {
    return _runner ? 1.0 / _runner->Settings().stepsPerSecond : fallback;
}
void WorkerSimulation::PrepareRewind() {
    if (_runner) _tape = _runner->TakeTape();
}
void WorkerSimulation::Forget() {
    _tape.reset();
    _loadedStart.reset();
    _lastError.clear();
}
bool WorkerSimulation::Load(const std::filesystem::path& file) {
    if (!_runner) {
        _lastError = "The open scene has no simulation";
        return false;
    }
    try {
        auto tape = InputTape::Load(file);
        if (auto problem = tape.Incompatibility(_runner->Settings()); !problem.empty()) {
            _lastError = "Cannot play this replay: " + std::move(problem);
            return false;
        }
        _loadedStart = tape.start;
        _tape = std::move(tape);
        _lastError.clear();
        return true;
    } catch (const std::exception& error) {
        _lastError = std::string("Cannot read the replay: ") + error.what();
        return false;
    }
}
bool WorkerSimulation::Save(const std::filesystem::path& file) {
    if (!_runner) {
        _lastError = "The open scene has no simulation";
        return false;
    }
    try {
        _runner->Tape().Save(file);
        _lastError.clear();
        return true;
    } catch (const std::exception& error) {
        _lastError = std::string("Cannot write the replay: ") + error.what();
        return false;
    }
}
nlohmann::json WorkerSimulation::State() const {
    if (!_runner) return _lastError.empty() ? nlohmann::json() : nlohmann::json{{"error", _lastError}};
    nlohmann::json state{{"step", _runner->StepIndex()},
                         {"stepsPerSecond", _runner->Settings().stepsPerSecond},
                         {"tapeSteps", _runner->Tape().steps},
                         {"replay", _runner->Replaying() ? "playing" : "live"}};
    if (const auto& mismatch = _runner->Mismatch()) {
        state["mismatch"] = {
            {"firstMismatch", mismatch->firstMismatch},
            {"lastMatch", mismatch->lastMatch ? nlohmann::json(*mismatch->lastMatch) : nlohmann::json()}};
    }
    if (!_lastError.empty()) state["error"] = _lastError;
    return state;
}
