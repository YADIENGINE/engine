#include "Menus/GridMenuSelection.h"
#include <algorithm>

GridMenuSelection::GridMenuSelection(int itemCount) { SetItemCount(itemCount); }

void GridMenuSelection::SetItemCount(int itemCount) {
    _itemCount = std::max(0, itemCount);
    if (_itemCount == 0) {
        _selected = -1;
        _focus = GridMenuFocus::Footer;
        return;
    }
    _selected = std::clamp(_selected, 0, _itemCount - 1);
}

void GridMenuSelection::Select(int index) {
    if (_itemCount == 0) return;
    _selected = std::clamp(index, 0, _itemCount - 1);
    _focus = GridMenuFocus::Grid;
}

void GridMenuSelection::Focus(GridMenuFocus focus) { _focus = _itemCount == 0 ? GridMenuFocus::Footer : focus; }

GridMenuMove GridMenuSelection::Move(int dx, int dy, int columns) {
    if (columns <= 0 || _itemCount == 0) return GridMenuMove::Unchanged;
    if (_focus == GridMenuFocus::Footer) {
        if (dy >= 0) return GridMenuMove::Unchanged;
        _focus = GridMenuFocus::Grid;
        return GridMenuMove::Changed;
    }
    if (dx != 0) {
        const int step = std::clamp(dx, -_selected, _itemCount - 1 - _selected);
        if (step == 0) return GridMenuMove::Unchanged;
        _selected += step;
        return GridMenuMove::Changed;
    }
    if (dy < 0) {
        if (_selected < columns) return GridMenuMove::ExitAbove;
        _selected -= columns;
        return GridMenuMove::Changed;
    }
    if (dy > 0) {
        if (_selected / columns < (_itemCount - 1) / columns) {
            _selected += std::min(columns, _itemCount - 1 - _selected);
        } else {
            _focus = GridMenuFocus::Footer;
        }
        return GridMenuMove::Changed;
    }
    return GridMenuMove::Unchanged;
}
