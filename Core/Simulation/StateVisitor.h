#pragma once
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>
#include "Simulation/StepSmoothed.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Random/Rng.h"
#include "Utilities/Geometry/Vec2.h"
#include "Utilities/Reflection.h"

class StateVisitor;

// A type that writes its own state: tweens, animators, cameras, collision worlds, a game's own classes.
template <class T>
concept VisitsOwnState = requires(const T& value, StateVisitor& state) { value.State(state); };
// A type StateVisitor writes as a value of its own, never field by field.
template <class T>
concept StateLeaf = std::is_arithmetic_v<T> || std::is_enum_v<T> || std::convertible_to<const T&, std::string_view> ||
                    std::same_as<T, Float2> || std::same_as<T, Vec2> || std::same_as<T, Color4> || std::same_as<T, Rng>;

using StateValue = std::variant<bool, std::int64_t, std::uint64_t, float, double, std::string_view>;

// Walks a simulation's state field by field. The hash and the readable dump are visitors, so they cannot disagree about
// what the state is. Fields are visited in a fixed order; never pass raw struct bytes, because padding changes between
// runs of one program.
//
// Keep what decides the game in a plain struct and visit it whole; every field it has is hashed and dumped by its own
// name, so a field added later cannot be forgotten:
//
//     struct RunState {
//         Rng rng;
//         StepSmoothed<float> shipX;   // Its current value
//         StepTween<float> recoil{0};  // Anything with State(visitor) writes its own group
//         std::vector<Enemy> enemies;  // enemies[0].hp, ... (Enemy is a plain struct too)
//         int score = 0;
//     };
//     void MySimulation::State(StateVisitor& state) const { state.Fields(_run); } // rng.state, shipX, score, ...
//
// What only the view needs (drawing caches, sound handles) stays outside that struct. Dumps follow declaration order,
// so moving a field changes the hash; tapes belong to one build anyway.
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
    // A value the view smooths between steps: only its current value decides anything.
    template <class T> void Field(std::string_view name, const StepSmoothed<T>& value) { Field(name, value.Current()); }
    // A group of what the value's own State writes: state.Field("animator", _animator).
    template <VisitsOwnState T> void Field(std::string_view name, const T& value) {
        Group(name, [&] { value.State(*this); });
    }
    // A group of every field of a plain struct (a pose, a game's state): name.field in dumps.
    template <PlainStruct T>
        requires(!VisitsOwnState<T> && !StateLeaf<T>)
    void Field(std::string_view name, const T& value) {
        Group(name, [&] { Fields(value); });
    }
    // One item group per element: name[0], name[1].x, ...
    template <class T> void Field(std::string_view name, const std::vector<T>& items) {
        for (std::size_t i = 0; i < items.size(); ++i)
            Group(name, i, [&] { Item(items[i]); });
    }
    template <class T, std::size_t N> void Field(std::string_view name, const std::array<T, N>& items) {
        for (std::size_t i = 0; i < N; ++i)
            Group(name, i, [&] { Item(items[i]); });
    }
    // A group that is empty when there is no value.
    template <class T> void Field(std::string_view name, const std::optional<T>& value) {
        Group(name, [&] {
            if (value) Item(*value);
        });
    }
    // A pointer is not state: visit what it points to, or an id.
    template <class T> void Field(std::string_view name, T* value) = delete;

    // Every field of a plain struct at this level, without a group of its own: state.Fields(_run) dumps "score".
    template <PlainStruct T> void Fields(const T& value) {
        ForEachField(value, [this](std::string_view name, const auto& field) { Field(name, field); });
    }

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
    // A value with no name of its own (a list item, an optional's value) is its group: dumps show name[2], not
    // name[2].something.
    template <class T> void Item(const T& value) {
        if constexpr (VisitsOwnState<T>)
            value.State(*this);
        else if constexpr (PlainStruct<T> && !StateLeaf<T>)
            Fields(value);
        else
            Field(std::string_view{}, value);
    }
    virtual void Value(std::string_view name, const StateValue& value) = 0;
    // 'index' is negative for a plain group.
    virtual void BeginGroup(std::string_view name, std::int64_t index) = 0;
    virtual void EndGroup() = 0;
};
