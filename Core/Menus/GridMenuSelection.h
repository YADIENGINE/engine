#pragma once

enum class GridMenuFocus { Grid, Footer };
enum class GridMenuMove { Unchanged, Changed, ExitAbove };

// Satır/sütun gezinmesi ve tek alt düğme odağı; öğelerin anlamı ve üst bölge çağırana aittir.
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
