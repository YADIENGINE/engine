#pragma once
#include "Procedural/MapCell.h"
#include "Utilities/Geometry/Grid.h"
#include "Utilities/Random/Rng.h"

// Caves from noise smoothed by a cellular automaton: cells start as wall by chance, then each round a cell becomes wall
// when five of its eight neighbours are (four, if it is a wall already), which grows rounded caverns. Floor areas
// smaller than 'minRegion' are filled and the rest joined by tunnels (Regions.h), so every floor cell can reach every
// other. Whole numbers only, apart from the starting chance, so a seed gives the same caves in every build of the same
// program.
//     const Grid<MapCell> map = GenerateCaves({.size = {80, 45}, .wallChance = 0.47f}, rng);
struct CaveSettings {
    Vec2 size{64, 40};        // The map, in cells
    float wallChance = 0.45f; // Share of cells that start as wall: more gives narrower caves (about 0.4..0.55)
    int smoothing = 5;        // Rounds of the rule; more gives smoother walls
    int minRegion = 12;       // Floor areas with fewer cells become wall before the rest are joined
};

[[nodiscard]] Grid<MapCell> GenerateCaves(const CaveSettings& settings, Rng& rng);
