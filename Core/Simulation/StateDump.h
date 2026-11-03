#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "Modules/JsonIO/JsonIO.h"
#include "Simulation/StateVisitor.h"

class Simulation;

// A state as a flat JSON object: {"score": 10, "player[0].lives": 3, "boss.position.x": 12.5}.
class StateDump final : public StateVisitor {
public:
    StateDump();
    [[nodiscard]] const JsonIO::Document& Result() const { return _result; }

protected:
    void Value(std::string_view name, const StateValue& value) override;
    void BeginGroup(std::string_view name, std::int64_t index) override;
    void EndGroup() override;

private:
    JsonIO::Document _result;
    std::vector<std::size_t> _prefixEnds;
    std::string _prefix;
};

[[nodiscard]] JsonIO::Document DumpState(const Simulation& simulation);
