#include "Menus/HeaderGridNavigation.h"
#include <algorithm>
#include <cassert>

HeaderGridNavigation::HeaderGridNavigation(int count, std::initializer_list<int> headerSizes)
    : _grid(count)
    , _sizes(headerSizes)
    , _cursors(_sizes.size(), 0) {
    assert(!_sizes.empty() && std::ranges::none_of(_sizes, [](int size) { return size <= 0; }));
}
void HeaderGridNavigation::ResetHeaders() { std::ranges::fill(_cursors, 0); }
void HeaderGridNavigation::SetItemCount(int count) {
    _grid.SetItemCount(count);
    if (ItemCount() == 0 && _focus == HeaderGridFocus::Grid) _focus = HeaderGridFocus::Footer;
}
void HeaderGridNavigation::SelectIndex(int index) {
    _grid.Select(index);
    if (ItemCount() > 0 && _focus == HeaderGridFocus::Footer) _focus = HeaderGridFocus::Grid;
}
void HeaderGridNavigation::Select(int index) {
    if (ItemCount() == 0) return;
    _grid.Select(index);
    _focus = HeaderGridFocus::Grid;
}
void HeaderGridNavigation::Focus(HeaderGridFocus focus, int row, int index) {
    _focus = focus == HeaderGridFocus::Grid && ItemCount() == 0 ? HeaderGridFocus::Empty : focus;
    if (_focus == HeaderGridFocus::Header) {
        _row = std::clamp(row, 0, static_cast<int>(_sizes.size()) - 1);
        if (index >= 0)
            _cursors[static_cast<std::size_t>(_row)] = std::clamp(index, 0, _sizes[static_cast<std::size_t>(_row)] - 1);
    }
    if (_focus == HeaderGridFocus::Grid || _focus == HeaderGridFocus::Footer)
        _grid.Focus(_focus == HeaderGridFocus::Grid ? GridMenuFocus::Grid : GridMenuFocus::Footer);
}
int HeaderGridNavigation::HeaderIndex(int row) const { return _cursors.at(static_cast<std::size_t>(row)); }
bool HeaderGridNavigation::Move(int dx, int dy, int columns) {
    if (columns <= 0) return false;
    if (_focus == HeaderGridFocus::Header) {
        auto& cursor = _cursors[static_cast<std::size_t>(_row)];
        if (dx != 0) {
            const int next = std::clamp(cursor + dx, 0, _sizes[static_cast<std::size_t>(_row)] - 1);
            if (next == cursor) return false;
            cursor = next;
            return true;
        }
        if (dy < 0 && _row > 0) {
            --_row;
            return true;
        }
        if (dy > 0) {
            if (_row + 1 < static_cast<int>(_sizes.size()))
                ++_row;
            else
                Focus(HeaderGridFocus::Grid);
            return true;
        }
        return false;
    }
    if (_focus == HeaderGridFocus::Empty) {
        if (dy == 0) return false;
        Focus(dy < 0 ? HeaderGridFocus::Header : HeaderGridFocus::Footer, static_cast<int>(_sizes.size()) - 1);
        return true;
    }
    if (_focus == HeaderGridFocus::Footer && ItemCount() == 0) {
        if (dy >= 0) return false;
        Focus(HeaderGridFocus::Empty);
        return true;
    }
    const auto moved = _grid.Move(dx, dy, columns);
    if (moved == GridMenuMove::ExitAbove) {
        Focus(HeaderGridFocus::Header, static_cast<int>(_sizes.size()) - 1);
        return true;
    }
    if (moved == GridMenuMove::Changed)
        _focus = _grid.Focus() == GridMenuFocus::Grid ? HeaderGridFocus::Grid : HeaderGridFocus::Footer;
    return moved == GridMenuMove::Changed;
}
