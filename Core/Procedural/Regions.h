#pragma once
#include <cstddef>
#include <vector>
#include "Procedural/MapCell.h"
#include "Utilities/Geometry/Grid.h"

// Floor areas of a layout: cells that reach each other through floor, side by side (diagonals do not join). Use them to
// tidy a map of your own or to find where to place things; the generators call them already.

// Every floor area, largest first (equal sizes in the order a row-by-row scan meets them); each area's cells in the
// order a flood fill from its first cell reaches them.
[[nodiscard]] std::vector<std::vector<Vec2>> FloorRegions(const Grid<MapCell>& map);
// Floor areas with fewer than 'minCells' cells become wall.
void FillSmallRegions(Grid<MapCell>& map, std::size_t minCells);
// Digs tunnels one cell wide through wall until every floor cell can reach every other: each tunnel is a shortest way
// from the largest area's side to the nearest other area. The map's border is never dug.
void ConnectRegions(Grid<MapCell>& map);
