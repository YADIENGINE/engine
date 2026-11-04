#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <variant>
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"
#include "Utilities/Rng.h"
#include "Utilities/Vec2.h"

using StateValue = std::variant<bool, std::int64_t, std::uint64_t, float, double, std::string_view>;

// Walks a simulation's state field by field. The hash and the readable dump are visitors, so they cannot disagree about
// what the state is. Fields are visited in a fixed order; never pass raw struct bytes, because padding changes between
// runs of one program.
class StateVisitor {
public:
    virtual ~StateVisitor();

    void Field(std::string_view name, bool value) { Value(name, value); }
    void Field(std::string_view name, float value) { Value(name, value); }
    void Field(std::string_view name, double value) { Value(name, value); }
    void Field(std::string_view name, std::string_view value) { Value(name, value); }
    void Field(std::string_view name, const char* value) { Value(name, std::string_view(value)); }
    template <std::signed_integral T> void Field(std::string_view name, T value) {
        Value(name, static_cast<std::int64_t>(value));
    }
    template <std::unsigned_integral T>
        requires(!std::same_as<T, bool>)
    void Field(std::string_view name, T value) {
        Value(name, static_cast<std::uint64_t>(value));
    }
    template <class T>
        requires std::is_enum_v<T>
    void Field(std::string_view name, T value) {
        Value(name, static_cast<std::int64_t>(value));
    }
    void Field(std::string_view name, Float2 value);
    void Field(std::string_view name, Vec2 value);
    void Field(std::string_view name, Color4 value);
    void Field(std::string_view name, const Rng& value);

    // A named part of the state; its fields appear as name.field in dumps.
    template <class Body> void Group(std::string_view name, Body&& body) {
        BeginGroup(name, -1);
        body();
        EndGroup();
    }
    // One item of a list; its fields appear as name[index].field in dumps.
    template <class Body> void Group(std::string_view name, std::size_t index, Body&& body) {
        BeginGroup(name, static_cast<std::int64_t>(index));
        body();
        EndGroup();
    }

protected:
    StateVisitor() = default;
    virtual void Value(std::string_view name, const StateValue& value) = 0;
    // 'index' is negative for a plain group.
    virtual void BeginGroup(std::string_view name, std::int64_t index) = 0;
    virtual void EndGroup() = 0;
};
