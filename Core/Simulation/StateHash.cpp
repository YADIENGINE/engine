#include "Simulation/StateHash.h"
#include <array>
#include <cstdio>
#include "Simulation/Simulation.h"

namespace {
constexpr std::uint64_t FnvPrime = 0x100000001b3ULL;
constexpr std::uint8_t GroupOpen = 0xF0, GroupClose = 0xF1;
} // namespace

void StateHash::Bytes(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) {
        _value ^= bytes[i];
        _value *= FnvPrime;
    }
}
void StateHash::Value(std::string_view, const StateValue& value) {
    const auto kind = static_cast<std::uint8_t>(value.index());
    Bytes(&kind, sizeof(kind));
    std::visit(
        [this](const auto& item) {
            using Item = std::decay_t<decltype(item)>;
            if constexpr (std::same_as<Item, std::string_view>) {
                const std::uint64_t size = item.size();
                Bytes(&size, sizeof(size));
                Bytes(item.data(), item.size());
            } else {
                Bytes(&item, sizeof(item));
            }
        },
        value);
}
void StateHash::BeginGroup(std::string_view, std::int64_t index) {
    Bytes(&GroupOpen, sizeof(GroupOpen));
    Bytes(&index, sizeof(index));
}
void StateHash::EndGroup() { Bytes(&GroupClose, sizeof(GroupClose)); }

std::uint64_t HashState(const Simulation& simulation) {
    StateHash hash;
    simulation.State(hash);
    return hash.Result();
}
std::string FormatStateHash(std::uint64_t hash) {
    std::array<char, 17> text{};
    std::snprintf(text.data(), text.size(), "%016llx", static_cast<unsigned long long>(hash));
    return text.data();
}
