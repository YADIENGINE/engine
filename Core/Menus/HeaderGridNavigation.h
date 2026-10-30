#pragma once
#include <initializer_list>
#include <vector>
#include "Menus/GridMenuSelection.h"

enum class HeaderGridFocus { Header, Grid, Empty, Footer };

// Grid seçimi + üstündeki başlık satırları (filtreler), boş durum eylemi ve footer arasında odak.
// Her başlık satırının en az bir öğesi olmalı.
class HeaderGridNavigation {
public:
    HeaderGridNavigation(int count, std::initializer_list<int> headerSizes);
    // Her başlık satırının imleci ilk öğeye döner; odak değişmez.
    void ResetHeaders();
    [[nodiscard]] int ItemCount() const { return _grid.ItemCount(); }
    void SetItemCount(int count);
    [[nodiscard]] int Selected() const { return _grid.Selected(); }
    void Select(int index);
    // Rebuilding content preserves an external header/empty focus.
    void SelectIndex(int index);
    [[nodiscard]] HeaderGridFocus Focus() const { return _focus; }
    void Focus(HeaderGridFocus focus, int row = 0, int index = -1);
    [[nodiscard]] int HeaderRow() const { return _row; }
    [[nodiscard]] int HeaderIndex(int row) const;
    bool Move(int dx, int dy, int columns);

private:
    GridMenuSelection _grid;
    std::vector<int> _sizes;
    std::vector<int> _cursors;
    HeaderGridFocus _focus = HeaderGridFocus::Grid;
    int _row = 0;
};
