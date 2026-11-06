#pragma once
#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <vector>
#include "Utilities/Geometry/Rect.h"
#include "Utilities/Geometry/Vec2.h"

// A rectangle of cells, each holding a T, addressed by Vec2 from {0, 0} at the top left. Plain data: copies are deep,
// two grids compare equal when their sizes and cells do, and a simulation keeps one in its state with
// state.Field("map", _map). Generated layouts (Procedural/) use it; a tile map can too. Header-only.
//     Grid<MapCell> map({40, 25}, MapCell::Wall);
//     map[{3, 4}] = MapCell::Floor;
//     if (map.Contains(next) && map[next] == MapCell::Floor) ...
template <class T> class Grid {
    static_assert(!std::is_same_v<T, bool>, "Grid<bool> would be std::vector<bool>; use an enum or std::uint8_t");

public:
    Grid() = default;
    // A size with a side below 1 gives an empty grid.
    explicit Grid(Vec2 size, const T& fill = T{})
        : _size(size.x > 0 && size.y > 0 ? size : Vec2{})
        , _cells(static_cast<std::size_t>(_size.x) * static_cast<std::size_t>(_size.y), fill) {}

    [[nodiscard]] Vec2 Size() const { return _size; }
    [[nodiscard]] int Width() const { return _size.x; }
    [[nodiscard]] int Height() const { return _size.y; }
    [[nodiscard]] bool Empty() const { return _cells.empty(); }
    [[nodiscard]] Rect Bounds() const { return {0, 0, _size.x, _size.y}; }
    [[nodiscard]] bool Contains(Vec2 cell) const {
        return cell.x >= 0 && cell.y >= 0 && cell.x < _size.x && cell.y < _size.y;
    }

    // The cell at 'cell', which must be inside (Contains).
    [[nodiscard]] T& operator[](Vec2 cell) { return _cells[Offset(cell)]; }
    [[nodiscard]] const T& operator[](Vec2 cell) const { return _cells[Offset(cell)]; }
    // The cell at 'cell', or 'outside' when it is not in the grid: neighbour counts treat the border as walls this way.
    [[nodiscard]] T At(Vec2 cell, const T& outside) const { return Contains(cell) ? (*this)[cell] : outside; }

    void Fill(const T& value) { std::ranges::fill(_cells, value); }
    // Every cell inside 'area' (clipped to the grid) set to 'value'.
    void Fill(const Rect& area, const T& value) {
        const int left = std::max(area.x, 0), top = std::max(area.y, 0);
        const int right = std::min(area.Right(), _size.x), bottom = std::min(area.Bottom(), _size.y);
        for (int y = top; y < bottom; ++y)
            for (int x = left; x < right; ++x)
                (*this)[{x, y}] = value;
    }
    [[nodiscard]] std::size_t Count(const T& value) const {
        return static_cast<std::size_t>(std::ranges::count(_cells, value));
    }

    // Row by row from the top left: the cell at {x, y} is Cells()[y * Width() + x].
    [[nodiscard]] const std::vector<T>& Cells() const { return _cells; }

    friend bool operator==(const Grid&, const Grid&) = default;

private:
    [[nodiscard]] std::size_t Offset(Vec2 cell) const {
        return static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(_size.x) + static_cast<std::size_t>(cell.x);
    }
    Vec2 _size{};
    std::vector<T> _cells;
};
