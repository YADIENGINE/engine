#include "Simulation/ReplayCheck.h"
#include <algorithm>
#include <memory>
#include <stdexcept>
#include "Simulation/StateDump.h"
#include "Simulation/StateHash.h"

namespace {
struct TapeRun {
    TapeRun(const SimulationDefinition& definition, const InputTape& tape)
        : simulation(definition.create(tape.start))
        , runner((Require(simulation), *simulation), definition.settings, tape.start) {
        runner.Play(tape, AfterTape::Stop);
    }
    static void Require(const std::unique_ptr<Simulation>& simulation) {
        if (!simulation) throw std::runtime_error("The simulation factory returned nothing");
    }
    std::unique_ptr<Simulation> simulation;
    SimulationRunner runner;
};
void RequirePlayable(const SimulationDefinition& definition, const InputTape& tape) {
    if (const auto problem = tape.Incompatibility(definition.settings); !problem.empty())
        throw std::invalid_argument(problem);
}
} // namespace

TapeVerification VerifyTape(const SimulationDefinition& definition, const InputTape& tape) {
    RequirePlayable(definition, tape);
    TapeRun run(definition, tape);
    run.runner.RunTo(tape.steps);
    (void)run.runner.RunStep(); // Past the end: covers the start's hash on an empty tape, and runs nothing
    TapeVerification result;
    result.steps = run.runner.StepIndex();
    result.hash = HashState(*run.simulation);
    result.hashes =
        std::min<std::uint64_t>(tape.hashes.size(), tape.steps / static_cast<std::uint64_t>(tape.hashEvery) + 1U);
    result.mismatch = run.runner.Mismatch();
    result.sameVersions = tape.SameVersions(definition.settings);
    result.tuningChanged = TuningChanged(tape);
    return result;
}
std::vector<std::string> TuningChanged(const InputTape& tape) {
    std::vector<std::string> changed;
    const auto now = TuningDigests();
    for (const auto& recorded : tape.tuning) {
        const auto current = std::ranges::find(now, recorded.source, &TuningDigest::source);
        if (current != now.end() && current->hash != recorded.hash) changed.push_back(recorded.source);
    }
    return changed;
}
std::optional<RunDivergence> FindDivergence(const SimulationDefinition& definition, const InputTape& tape) {
    RequirePlayable(definition, tape);
    TapeRun first(definition, tape);
    TapeRun second(definition, tape);
    std::uint64_t step = 0;
    for (;;) {
        if (HashState(*first.simulation) != HashState(*second.simulation)) {
            RunDivergence divergence;
            divergence.step = step;
            const auto a = DumpState(*first.simulation), b = DumpState(*second.simulation);
            for (const auto& [path, value] : a.Members()) {
                const auto& others = b.Members();
                const auto other = others.find(path);
                const JsonIO::Document missing;
                const auto& compared = other == others.end() ? missing : other->second;
                if (!(value == compared)) divergence.fields.push_back({path, value, compared});
            }
            for (const auto& [path, value] : b.Members())
                if (!a.Members().contains(path)) divergence.fields.push_back({path, nullptr, value});
            return divergence;
        }
        if (step >= tape.steps) return std::nullopt;
        (void)first.runner.RunStep();
        (void)second.runner.RunStep();
        ++step;
    }
}
TapeState StateAt(const SimulationDefinition& definition, const InputTape& tape, std::uint64_t step) {
    RequirePlayable(definition, tape);
    TapeRun run(definition, tape);
    run.runner.RunTo(step);
    return {run.runner.StepIndex(), HashState(*run.simulation), DumpState(*run.simulation)};
}
