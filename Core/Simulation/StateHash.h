#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include "Simulation/StateVisitor.h"

class Simulation;

// FNV-1a 64 over a state's values and structure; field names are left out, so hashing is cheap. Floats hash by their
// exact bits: a difference in the last bit is a difference.
class StateHash final : public StateVisitor {
public:
    [[nodiscard]] std::uint64_t Result() const { return _value; }

protected:
    void Value(std::string_view name, const StateValue& value) override;
    void BeginGroup(std::string_view name, std::int64_t index) override;
    void EndGroup() override;

private:
    void Bytes(const void* data, std::size_t size);
    std::uint64_t _value = 0xcbf29ce484222325ULL;
};

[[nodiscard]] std::uint64_t HashState(const Simulation& simulation);
// 16 lowercase hexadecimal digits, the form tapes and tools use.
[[nodiscard]] std::string FormatStateHash(std::uint64_t hash);
