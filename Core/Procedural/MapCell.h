#pragma once
#include <cstdint>

// What a generated layout says about a cell (Procedural/): a Grid<MapCell> is the layout, and the game decides what
// each kind becomes (tiles, colliders, decoration). Generators keep the map's border wall.
enum class MapCell : std::uint8_t { Wall, Floor };
