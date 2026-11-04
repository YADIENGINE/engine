#pragma once
#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

// An element's name among its siblings: a string, a number or an enum value. Elements find their state and focus from
// one frame to the next by it, so it must stay the same while the element is on screen: use a key from the data rather
// than the translated text. The full identity also includes the parents' ids.
class UiId {
public:
    constexpr UiId() = default;
    constexpr UiId(const char* name)
        : UiId(std::string_view(name)) {}
    constexpr UiId(std::string_view name) {
        for (const char character : name) Mix(static_cast<std::uint8_t>(character));
    }
    UiId(const std::string& name)
        : UiId(std::string_view(name)) {}
    template <std::integral Number>
    constexpr UiId(Number number)
        : UiId(static_cast<std::uint64_t>(number), Raw{}) {}
    template <class Enum>
        requires std::is_enum_v<Enum>
    constexpr UiId(Enum value)
        : UiId(static_cast<std::uint64_t>(value), Raw{}) {}

    [[nodiscard]] constexpr std::uint64_t Value() const { return _value; }
    [[nodiscard]] constexpr UiId Under(UiId parent) const {
        UiId result = parent;
        result.MixWord(_value);
        return result;
    }
    friend constexpr bool operator==(UiId, UiId) = default;

private:
    struct Raw {};
    constexpr UiId(std::uint64_t number, Raw) { MixWord(number); }
    constexpr void Mix(std::uint8_t byte) {
        _value ^= byte;
        _value *= 0x100000001B3ULL;
    }
    constexpr void MixWord(std::uint64_t word) {
        for (int shift = 0; shift < 64; shift += 8) Mix(static_cast<std::uint8_t>(word >> shift));
    }

    std::uint64_t _value = 0xCBF29CE484222325ULL; // FNV-1a
};
