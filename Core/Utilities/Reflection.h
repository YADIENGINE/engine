#pragma once
#include <pfr/core.hpp>
#include <pfr/core_name.hpp>
#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

// std::array is an aggregate, but a list rather than a struct of named fields.
template <class T> inline constexpr bool IsStdArray = false;
template <class T, std::size_t N> inline constexpr bool IsStdArray<std::array<T, N>> = true;

// A plain struct tells the engine its fields: their names, in the order they are declared, with no list to keep. Poses
// blend this way and simulation state reaches State this way, so a field added to the struct is never left out:
//
//     struct RunState {
//         Rng rng;
//         float shipX = 0.0f;
//         int score = 0;
//     };
//     ForEachField(run, [](std::string_view name, const auto& field) { ... }); // "rng", "shipX", "score"
//
// A plain struct has public fields only: no base class, no constructor, no virtual function, and no reference, const
// or C array field (std::array is fine). Static members and member functions are not fields. It needs a name outside
// any function and outside an anonymous namespace, because field names are read from the type at compile time; if not,
// the build stops at "do_not_use_PFR_with_local_types".
//
// PFR does the work; this is the only file that names it (python Engine check).
template <class T>
concept PlainStruct = std::is_class_v<T> && std::is_aggregate_v<T> && !std::is_polymorphic_v<T> && !IsStdArray<T>;

// How many fields a plain struct has.
template <PlainStruct T> inline constexpr std::size_t FieldCount = pfr::tuple_size_v<T>;
// The type of field I, as declared.
template <std::size_t I, PlainStruct T> using FieldType = pfr::tuple_element_t<I, T>;
// The name of field I, as written in the struct.
template <std::size_t I, PlainStruct T> [[nodiscard]] constexpr std::string_view FieldName() {
    return pfr::get_name<I, T>();
}
// Field I of a value.
template <std::size_t I, PlainStruct T> [[nodiscard]] constexpr auto& FieldAt(T& value) { return pfr::get<I>(value); }
template <std::size_t I, PlainStruct T> [[nodiscard]] constexpr const auto& FieldAt(const T& value) {
    return pfr::get<I>(value);
}

// body(name, field) for every field, in declaration order. 'value' may be const; then each field is too.
template <class S, class Body>
    requires PlainStruct<std::remove_const_t<S>>
constexpr void ForEachField(S& value, Body&& body) {
    using T = std::remove_const_t<S>;
    [&]<std::size_t... I>(std::index_sequence<I...>) {
        (body(FieldName<I, T>(), FieldAt<I>(value)), ...);
    }(std::make_index_sequence<FieldCount<T>>{});
}
