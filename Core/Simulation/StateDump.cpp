#include "Simulation/StateDump.h"
#include <type_traits>
#include <utility>
#include "Simulation/Simulation.h"

StateDump::StateDump()
    : _result(JsonIO::Document::MakeObject()) {}
namespace {
// An unnamed value or group is its enclosing group itself: "items[2]" rather than "items[2].".
void Append(std::string& path, std::string_view name) {
    if (name.empty() && !path.empty() && path.back() == '.') path.pop_back();
    path += name;
}
} // namespace

void StateDump::Value(std::string_view name, const StateValue& value) {
    auto path = _prefix;
    Append(path, name);
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
    Append(_prefix, name);
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
