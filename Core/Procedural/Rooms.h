#pragma once
#include <vector>
#include "Procedural/MapCell.h"
#include "Utilities/Geometry/Grid.h"
#include "Utilities/Geometry/Rect.h"
#include "Utilities/Random/Rng.h"

// Rooms joined by corridors (a dungeon): the map is cut in two again and again (binary space partition) until each
// part holds one room, and the two halves of every cut are joined by an L-shaped corridor between their nearest
// rooms. Every room can reach every other; rooms are at least two walls apart. Whole numbers only, so a seed gives
// the same layout in every build.
//     Rng rng(start.seed, 1); // A stream of its own: the layout does not move the game's other rolls
//     const RoomLayout level = GenerateRooms({.size = {64, 40}}, rng);
//     const Vec2 spawn{level.rooms.front().CenterX(), level.rooms.front().CenterY()};
struct RoomSettings {
    Vec2 size{64, 40}; // The map, in cells
    int minRoom = 4;   // Shortest room side, in cells (at least 1)
    int maxRoom = 10;  // Longest room side; parts are cut until they are about this size
    int corridorWidth = 1;
};

// Two rooms a corridor joins, as indices into RoomLayout::rooms.
struct RoomLink {
    int from = 0;
    int to = 0;
};

struct RoomLayout {
    Grid<MapCell> cells;
    std::vector<Rect> rooms;     // In cells; in the order the cuts made them
    std::vector<RoomLink> links; // One fewer than the rooms: the corridors form a tree, so there are no loops
};

// A map too small for one room of 'minRoom' (plus its walls) is all wall, with no rooms.
[[nodiscard]] RoomLayout GenerateRooms(const RoomSettings& settings, Rng& rng);
