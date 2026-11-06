#pragma once
#include <optional>
#include "Procedural/MapCell.h"
#include "Utilities/Geometry/Grid.h"
#include "Utilities/Random/Rng.h"

// A walker digs floor out of solid wall, one random step up, down, left or right at a time ("drunkard's walk"), until
// enough of the map is floor: winding, organic tunnels and chambers, always connected. Whole numbers only, apart from
// one multiplication of 'floorShare', so a seed gives the same layout in every build of the same program.
//     const Grid<MapCell> map = GenerateRandomWalk({.size = {48, 32}, .floorShare = 0.35f}, rng);
struct RandomWalkSettings {
    Vec2 size{64, 40};         // The map, in cells
    float floorShare = 0.4f;   // Stops once this share of the cells inside the border is floor (0..1)
    std::optional<Vec2> start; // Where the walk begins (and so is floor); the map's centre if not given
};

// The walk also stops after 50 steps per cell inside the border, so a share near 1 cannot run forever. A map smaller
// than 3 by 3 has no inside and stays all wall.
[[nodiscard]] Grid<MapCell> GenerateRandomWalk(const RandomWalkSettings& settings, Rng& rng);
