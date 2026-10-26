#pragma once
#include <algorithm>
#include <optional>
#include "Utilities/Rect.h"

[[nodiscard]] constexpr bool Clicked(std::optional<Vec2> pointer, const Rect& rect) {
    return pointer && rect.Contains(*pointer);
}

template <class Menu> bool NavigateMenu(Menu& menu, int step) {
    if (step < 0) menu.SelectPrevious();
    if (step > 0) menu.SelectNext();
    return step != 0;
}

template <class Menu, class Area>
[[nodiscard]] auto ItemAt(const Menu& menu, std::optional<Vec2> pointer, Area area)
    -> std::optional<decltype(menu.Selected())> {
    int index = 0;
    for (const auto item : menu.AllItems()) {
        if (Clicked(pointer, area(index))) return item;
        ++index;
    }
    return std::nullopt;
}

template <class Menu> [[nodiscard]] int IndexOf(const Menu& menu, decltype(menu.Selected()) item) {
    const auto& items = menu.AllItems();
    return static_cast<int>(std::find(items.begin(), items.end(), item) - items.begin());
}
