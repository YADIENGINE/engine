#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include "Procedural/MapCell.h"
#include "Utilities/Geometry/Grid.h"

TEST_CASE("Engine.Grid.CellsAreAddressedRowByRow", "[engine][procedural]") {
    Grid<int> grid({4, 3}, 7);
    CHECK(grid.Width() == 4);
    CHECK(grid.Height() == 3);
    CHECK(grid.Cells().size() == 12);
    CHECK(grid.Count(7) == 12);
    grid[{2, 1}] = 5;
    CHECK(grid.Cells()[1 * 4 + 2] == 5);
    CHECK(grid.Contains({3, 2}));
    CHECK_FALSE(grid.Contains({4, 0}));
    CHECK_FALSE(grid.Contains({0, -1}));
    CHECK(grid.At({-1, 0}, -9) == -9);
    CHECK(grid.At({2, 1}, -9) == 5);
}
TEST_CASE("Engine.Grid.FillsAClippedRectangle", "[engine][procedural]") {
    Grid<MapCell> grid({5, 5}, MapCell::Wall);
    grid.Fill(Rect{3, 3, 10, 10}, MapCell::Floor); // Reaches past the grid
    CHECK(grid.Count(MapCell::Floor) == 4);
    CHECK(grid[{4, 4}] == MapCell::Floor);
    CHECK(grid[{2, 2}] == MapCell::Wall);
    grid.Fill(MapCell::Floor);
    CHECK(grid.Count(MapCell::Wall) == 0);
}
TEST_CASE("Engine.Grid.CopiesAreDeepAndCompareByContent", "[engine][procedural]") {
    Grid<std::uint8_t> first({3, 2}, 0);
    Grid<std::uint8_t> second = first;
    CHECK(first == second);
    second[{0, 0}] = 1;
    CHECK_FALSE(first == second);
    CHECK(first[{0, 0}] == 0);
    CHECK_FALSE(Grid<std::uint8_t>({3, 2}, 0) == Grid<std::uint8_t>({2, 3}, 0)); // Same cells, other shape
}
TEST_CASE("Engine.Grid.ABadSizeIsEmpty", "[engine][procedural]") {
    const Grid<int> grid({-3, 5}, 1);
    CHECK(grid.Empty());
    CHECK(grid.Size() == Vec2{});
    CHECK_FALSE(grid.Contains({0, 0}));
}
