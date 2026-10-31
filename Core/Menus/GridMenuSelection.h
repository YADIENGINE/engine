#pragma once

enum class GridMenuFocus { Grid, Footer };
enum class GridMenuMove { Unchanged, Changed, ExitAbove };

// Row/column navigation and focus on a single bottom button; item meanings and the upper region belong to the caller.
class GridMenuSelection {
public:
    explicit GridMenuSelection(int itemCount);
    [[nodiscard]] int ItemCount() const { return _itemCount; }
    void SetItemCount(int itemCount);
    [[nodiscard]] int Selected() const { return _selected; }
    void Select(int index);
    [[nodiscard]] GridMenuFocus Focus() const { return _focus; }
    void Focus(GridMenuFocus focus);
    [[nodiscard]] GridMenuMove Move(int dx, int dy, int columns);

private:
    int _itemCount = 0;
    int _selected = -1;
    GridMenuFocus _focus = GridMenuFocus::Grid;
};
