#include "Simulation/ScenarioScript.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include "Simulation/Scenario.h"
#include "Simulation/StateHash.h"

namespace {
constexpr double FloatTolerance = 1e-6;
constexpr std::uint64_t DefaultUntilLimit = 3600;

const JsonIO::Document* Find(const JsonIO::Document& object, std::string_view key) {
    const auto& members = object.Members();
    const auto found = members.find(key);
    return found == members.end() ? nullptr : &found->second;
}
bool IsNumber(const JsonIO::Document& value) {
    return std::holds_alternative<std::int64_t>(value.value) || std::holds_alternative<std::uint64_t>(value.value) ||
           std::holds_alternative<double>(value.value);
}
bool IsFloat(const JsonIO::Document& value) { return std::holds_alternative<double>(value.value); }
double NumberOf(const JsonIO::Document& value) {
    if (const auto* number = std::get_if<std::int64_t>(&value.value)) return static_cast<double>(*number);
    if (const auto* number = std::get_if<std::uint64_t>(&value.value)) return static_cast<double>(*number);
    return std::get<double>(value.value);
}
bool Matches(const JsonIO::Document& actual, const JsonIO::Document& expected) {
    if (std::holds_alternative<JsonIO::Document::Object>(expected.value)) {
        if (!IsNumber(actual)) return false;
        const double value = NumberOf(actual);
        if (const auto* low = Find(expected, "min"); low && value < NumberOf(*low)) return false;
        if (const auto* high = Find(expected, "max"); high && value > NumberOf(*high)) return false;
        return true;
    }
    if (IsNumber(actual) && IsNumber(expected)) {
        if (!IsFloat(actual) && !IsFloat(expected)) return NumberOf(actual) == NumberOf(expected);
        const double a = NumberOf(actual), b = NumberOf(expected);
        return std::fabs(a - b) <= FloatTolerance * std::max(1.0, std::fabs(b));
    }
    return actual == expected;
}
const JsonIO::Document& FieldOf(const JsonIO::Document& state, const std::string& path) {
    const auto* found = Find(state, path);
    if (!found) throw std::invalid_argument("The state has no field named " + path);
    return *found;
}
int PlayerOf(const JsonIO::Document& operation) {
    const auto* player = Find(operation, "player");
    return player ? static_cast<int>(player->AsInt64()) : 0;
}
class ScriptRun {
public:
    ScriptRun(const SimulationDefinition& definition, SimulationStart start)
        : _scenario(definition, std::move(start))
        , _failures(JsonIO::Document::MakeArray())
        , _reads(JsonIO::Document::MakeArray()) {}
    void Execute(const JsonIO::Document& operation) {
        if (const auto* run = Find(operation, "run")) {
            _scenario.Run(run->AsUInt64());
        } else if (const auto* hold = Find(operation, "hold")) {
            _scenario.Hold(hold->AsString(), PlayerOf(operation));
        } else if (const auto* release = Find(operation, "release")) {
            _scenario.Release(release->AsString(), PlayerOf(operation));
        } else if (const auto* tap = Find(operation, "tap")) {
            _scenario.Tap(tap->AsString(), PlayerOf(operation));
        } else if (const auto* axis = Find(operation, "axis")) {
            _scenario.SetAxis(axis->AsString(), static_cast<float>(NumberOf(operation.At("value"))),
                              PlayerOf(operation));
        } else if (const auto* command = Find(operation, "command")) {
            const auto* value = Find(operation, "value");
            _scenario.Command(command->AsString(), value ? value->AsInt64() : 0);
        } else if (const auto* expect = Find(operation, "expect")) {
            Expect(*expect);
        } else if (const auto* until = Find(operation, "until")) {
            const auto* limit = Find(operation, "limit");
            Until(*until, limit ? limit->AsUInt64() : DefaultUntilLimit);
        } else if (const auto* read = Find(operation, "read")) {
            Read(*read);
        } else {
            throw std::invalid_argument("Unknown scenario operation: " + JsonIO::Serialize(operation));
        }
    }
    [[nodiscard]] ScenarioResult Finish() {
        ScenarioResult result;
        result.passed = _failures.Elements().empty();
        auto report = JsonIO::Document::MakeObject();
        report["passed"] = result.passed;
        report["steps"] = _scenario.Step();
        report["hash"] = FormatStateHash(_scenario.Hash());
        report["failures"] = std::move(_failures);
        report["reads"] = std::move(_reads);
        result.report = std::move(report);
        result.tape = _scenario.Tape();
        return result;
    }

private:
    void Expect(const JsonIO::Document& expected) {
        const auto state = _scenario.State();
        for (const auto& [path, value] : expected.Members()) {
            const auto& actual = FieldOf(state, path);
            if (Matches(actual, value)) continue;
            auto failure = JsonIO::Document::MakeObject();
            failure["step"] = _scenario.Step();
            failure["field"] = path;
            failure["expected"] = value;
            failure["actual"] = actual;
            _failures.Append(std::move(failure));
        }
    }
    void Until(const JsonIO::Document& expected, std::uint64_t limit) {
        for (std::uint64_t ran = 0;; ++ran) {
            const auto state = _scenario.State();
            bool reached = true;
            for (const auto& [path, value] : expected.Members())
                reached = reached && Matches(FieldOf(state, path), value);
            if (reached) return;
            if (ran == limit) {
                auto failure = JsonIO::Document::MakeObject();
                failure["step"] = _scenario.Step();
                failure["until"] = expected;
                failure["limit"] = limit;
                _failures.Append(std::move(failure));
                return;
            }
            _scenario.Run();
        }
    }
    void Read(const JsonIO::Document& paths) {
        const auto state = _scenario.State();
        auto values = JsonIO::Document::MakeObject();
        for (const auto& path : paths.Elements())
            values[path.AsString()] = FieldOf(state, path.AsString());
        auto entry = JsonIO::Document::MakeObject();
        entry["step"] = _scenario.Step();
        entry["hash"] = FormatStateHash(_scenario.Hash());
        entry["values"] = std::move(values);
        _reads.Append(std::move(entry));
    }
    Scenario _scenario;
    JsonIO::Document _failures;
    JsonIO::Document _reads;
};
} // namespace

ScenarioResult RunScenarioScript(const SimulationRegistry& registry, const JsonIO::Document& script) {
    const auto& id = script.At("simulation").AsString();
    const auto* definition = registry.Find(id);
    if (!definition) throw std::invalid_argument("Unknown simulation: " + id);
    SimulationStart start;
    if (const auto* seed = Find(script, "seed")) start.seed = seed->AsUInt64();
    if (const auto* players = Find(script, "players")) start.players = static_cast<int>(players->AsInt64());
    if (const auto* parameters = Find(script, "start")) start.parameters = *parameters;
    ScriptRun run(*definition, std::move(start));
    for (const auto& operation : script.At("script").Elements())
        run.Execute(operation);
    return run.Finish();
}
