#pragma once
#include <cstddef>
#include <iterator>
#include <type_traits>

// Enum sırasıyla yazılmış satır tabloları: her satırın 'id' alanı kendi sırasına eşit ve tablo enum'un bütün
// değerlerini (Id::Count) kapsar. Kullanım: static_assert(RowsInIdOrder(Rows)); yeni enum değeri satırı olmadan
// derlenmez. Header-only.
template <class Rows> constexpr bool RowsInIdOrder(const Rows& rows) {
    using Id = std::remove_cvref_t<decltype(std::begin(rows)->id)>;
    std::size_t index = 0;
    for (const auto& row : rows) {
        if (static_cast<std::size_t>(row.id) != index) return false;
        ++index;
    }
    return index == static_cast<std::size_t>(Id::Count);
}
