#pragma once
#include <array>
#include <cstddef>

// Dikey bir menünün seçim durumu: 'Items' sırasıyla dizilen seçeneklerden biri seçilidir. Saf mantık: input'u Game
// verir, çizimi menünün View'ı yapar. Header-only.
template <class Item, Item... Items> class MenuSelection {
public:
    void SelectNext() { _selectedIndex = (_selectedIndex + 1) % ItemList.size(); }
    void SelectPrevious() { _selectedIndex = (_selectedIndex + ItemList.size() - 1) % ItemList.size(); }
    void Select(Item item) {
        for (std::size_t index = 0; index < ItemList.size(); ++index) {
            if (ItemList[index] == item) {
                _selectedIndex = index;
                return;
            }
        }
    }

    [[nodiscard]] Item Selected() const { return ItemList[_selectedIndex]; }
    [[nodiscard]] static constexpr const std::array<Item, sizeof...(Items)>& AllItems() { return ItemList; }

private:
    static constexpr std::array<Item, sizeof...(Items)> ItemList{Items...};

    std::size_t _selectedIndex = 0;
};
