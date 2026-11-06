// The console tool behind `<engine> sim`: no window, no drawing. Each command prints one JSON object on stdout. Exit
// codes follow Docs/ToolsContract.md: 0 success, 1 a failed check, 2 usage or a tape that cannot play.
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Modules/JsonIO/JsonIO.h"
#include "Simulation/InputTape.h"
#include "Simulation/ReplayCheck.h"
#include "Simulation/ScenarioScript.h"
#include "Simulation/SimulationRegistry.h"
#include "Simulation/StateHash.h"
#include "Diagnostics/Internals/CrashSession.h"

namespace {
constexpr int ExitOk = 0, ExitFailed = 1, ExitUsage = 2;
constexpr JsonIO::Limits OutputLimits{.maxBytes = std::size_t{1} << 28U, .maxDepth = 16};
constexpr const char* Usage =
    "usage: list | run SCENARIO [--out TAPE] [--events] | verify TAPE | diverge TAPE | state TAPE --step N";

void Print(const JsonIO::Document& answer) {
    std::cout << JsonIO::Serialize(answer,
                                   JsonIO::SerializeOptions{.limits = OutputLimits, .format = JsonIO::Format::Compact})
              << '\n';
}
const SimulationDefinition& DefinitionFor(const SimulationRegistry& registry, const InputTape& tape) {
    const auto* definition = registry.Find(tape.simulation);
    if (!definition) throw std::invalid_argument("Unknown simulation: " + tape.simulation);
    return *definition;
}
std::optional<std::string> OptionValue(const std::vector<std::string_view>& arguments, std::string_view name) {
    for (std::size_t i = 0; i + 1 < arguments.size(); ++i)
        if (arguments[i] == name) return std::string(arguments[i + 1]);
    return std::nullopt;
}
int List(const SimulationRegistry& registry) {
    auto simulations = JsonIO::Document::MakeArray();
    for (const auto& entry : registry.Entries()) {
        auto item = JsonIO::Document::MakeObject();
        item["id"] = entry.settings.id;
        item["version"] = entry.settings.version;
        item["stepsPerSecond"] = entry.settings.stepsPerSecond;
        item["maxPlayers"] = entry.settings.maxPlayers;
        auto buttons = JsonIO::Document::MakeArray();
        for (const auto& name : entry.settings.buttons)
            buttons.Append(name);
        item["buttons"] = std::move(buttons);
        auto axes = JsonIO::Document::MakeArray();
        for (const auto& name : entry.settings.axes)
            axes.Append(name);
        item["axes"] = std::move(axes);
        simulations.Append(std::move(item));
    }
    auto answer = JsonIO::Document::MakeObject();
    answer["simulations"] = std::move(simulations);
    Print(answer);
    return ExitOk;
}
int Run(const SimulationRegistry& registry, const std::vector<std::string_view>& arguments) {
    const bool listEvents = std::ranges::find(arguments, std::string_view("--events")) != arguments.end();
    auto result = RunScenarioScript(registry, JsonIO::Read(std::filesystem::path(arguments.at(0))),
                                    ScenarioScriptOptions{.listEvents = listEvents});
    if (const auto out = OptionValue(arguments, "--out")) result.tape.Save(std::filesystem::path(*out));
    Print(result.report);
    return result.passed ? ExitOk : ExitFailed;
}
int Verify(const SimulationRegistry& registry, const std::vector<std::string_view>& arguments) {
    const auto tape = InputTape::Load(std::filesystem::path(arguments.at(0)));
    const auto result = VerifyTape(DefinitionFor(registry, tape), tape);
    auto answer = JsonIO::Document::MakeObject();
    const bool matched = result.hashes > 0 && !result.mismatch;
    answer["match"] = matched;
    answer["hashes"] = result.hashes;
    if (result.hashes == 0) answer["reason"] = "The tape holds no hashes, so nothing was compared";
    answer["steps"] = result.steps;
    answer["hash"] = FormatStateHash(result.hash);
    answer["hashEvery"] = tape.hashEvery;
    answer["sameVersions"] = result.sameVersions;
    if (!result.tuningChanged.empty()) {
        auto files = JsonIO::Document::MakeArray();
        for (const auto& file : result.tuningChanged)
            files.Append(file);
        answer["tuningChanged"] = std::move(files);
    }
    auto recorded = JsonIO::Document::MakeObject();
    recorded["engine"] = tape.engineVersion;
    recorded["simulationVersion"] = tape.simulationVersion;
    recorded["configuration"] = tape.configuration;
    answer["recorded"] = std::move(recorded);
    if (result.mismatch) {
        auto mismatch = JsonIO::Document::MakeObject();
        mismatch["firstMismatch"] = result.mismatch->firstMismatch;
        if (result.mismatch->lastMatch)
            mismatch["lastMatch"] = *result.mismatch->lastMatch;
        else
            mismatch["lastMatch"] = nullptr;
        answer["mismatch"] = std::move(mismatch);
    }
    Print(answer);
    return matched ? ExitOk : ExitFailed;
}
int Diverge(const SimulationRegistry& registry, const std::vector<std::string_view>& arguments) {
    const auto tape = InputTape::Load(std::filesystem::path(arguments.at(0)));
    const auto divergence = FindDivergence(DefinitionFor(registry, tape), tape);
    auto answer = JsonIO::Document::MakeObject();
    answer["deterministic"] = !divergence.has_value();
    answer["steps"] = tape.steps;
    if (divergence) {
        answer["step"] = divergence->step;
        auto fields = JsonIO::Document::MakeArray();
        for (const auto& field : divergence->fields) {
            auto item = JsonIO::Document::MakeObject();
            item["field"] = field.path;
            item["first"] = field.first;
            item["second"] = field.second;
            fields.Append(std::move(item));
        }
        answer["fields"] = std::move(fields);
    }
    Print(answer);
    return divergence ? ExitFailed : ExitOk;
}
int State(const SimulationRegistry& registry, const std::vector<std::string_view>& arguments) {
    const auto tape = InputTape::Load(std::filesystem::path(arguments.at(0)));
    std::uint64_t step = tape.steps;
    if (const auto text = OptionValue(arguments, "--step")) {
        const auto [end, error] = std::from_chars(text->data(), text->data() + text->size(), step);
        if (error != std::errc{} || end != text->data() + text->size())
            throw std::invalid_argument("--step needs a whole number");
    }
    const auto state = StateAt(DefinitionFor(registry, tape), tape, step);
    auto answer = JsonIO::Document::MakeObject();
    answer["step"] = state.step;
    answer["hash"] = FormatStateHash(state.hash);
    answer["state"] = state.state;
    Print(answer);
    return ExitOk;
}
int Dispatch(const std::vector<std::string_view>& arguments) {
    if (arguments.empty()) throw std::invalid_argument(Usage);
    SimulationRegistry registry;
    RegisterSimulations(registry);
    const auto command = arguments.front();
    const std::vector<std::string_view> rest(arguments.begin() + 1, arguments.end());
    if (command == "list") return List(registry);
    if (rest.empty()) throw std::invalid_argument(Usage);
    if (command == "run") return Run(registry, rest);
    if (command == "verify") return Verify(registry, rest);
    if (command == "diverge") return Diverge(registry, rest);
    if (command == "state") return State(registry, rest);
    throw std::invalid_argument(Usage);
}
} // namespace

int main(int argc, char** argv) {
    const CrashSession crash("simulation-tool");
    return RunCrashBoundary([&] {
        try {
            return Dispatch(std::vector<std::string_view>(argv + 1, argv + argc));
        } catch (const std::invalid_argument& error) { // A bad request: usage, a script, or a tape that cannot play
            std::fprintf(stderr, "error: %s\n", error.what());
            return ExitUsage;
        } catch (const JsonIO::Error& error) { // An unreadable file
            std::fprintf(stderr, "error: %s\n", error.what());
            return ExitUsage;
        }
    });
}
