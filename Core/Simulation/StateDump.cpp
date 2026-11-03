#include "Simulation/StateDump.h"
#include <type_traits>
#include <utility>
#include "Simulation/Simulation.h"

StateDump::StateDump()
    : _result(JsonIO::Document::MakeObject()) {}
void StateDump::Value(std::string_view name, const StateValue& value) {
    auto path = _prefix;
    path += name;
    _result[std::move(path)] = std::visit(
        [](const auto& item) -> JsonIO::Document {
            if constexpr (std::is_same_v<std::decay_t<decltype(item)>, float>)
                return static_cast<double>(item);
            else
                return item;
        },
        value);
}
void StateDump::BeginGroup(std::string_view name, std::int64_t index) {
    _prefixEnds.push_back(_prefix.size());
    _prefix += name;
    if (index >= 0) _prefix += '[' + std::to_string(index) + ']';
    _prefix += '.';
}
void StateDump::EndGroup() {
    _prefix.resize(_prefixEnds.back());
    _prefixEnds.pop_back();
}

JsonIO::Document DumpState(const Simulation& simulation) {
    StateDump dump;
    simulation.State(dump);
    return dump.Result();
}
