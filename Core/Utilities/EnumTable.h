#pragma once
#include <cstddef>
#include <iterator>
#include <type_traits>

// Row tables in enum order: each row's 'id' equals its index and the table covers all enum values (Id::Count).
// Usage: static_assert(RowsInIdOrder(Rows)); a new enum value without its row fails to compile. Header-only.
template <class Rows> constexpr bool RowsInIdOrder(const Rows& rows) {
    using Id = std::remove_cvref_t<decltype(std::begin(rows)->id)>;
    std::size_t index = 0;
    for (const auto& row : rows) {
        if (static_cast<std::size_t>(row.id) != index) return false;
        ++index;
    }
    return index == static_cast<std::size_t>(Id::Count);
}
