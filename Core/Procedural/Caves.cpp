#include "Procedural/Caves.h"
#include <algorithm>
#include <cstddef>
#include "Procedural/Regions.h"

namespace {
// Walls among the eight neighbours; outside the map counts as wall, so caves keep away from the edge.
int WallsAround(const Grid<MapCell>& map, Vec2 cell) {
    int walls = 0;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            if ((dx != 0 || dy != 0) && map.At(cell + Vec2{dx, dy}, MapCell::Wall) == MapCell::Wall) ++walls;
    return walls;
}
} // namespace

Grid<MapCell> GenerateCaves(const CaveSettings& settings, Rng& rng) {
    Grid<MapCell> map(settings.size, MapCell::Wall);
    if (map.Width() < 3 || map.Height() < 3) return map;
    const Rect inside{1, 1, map.Width() - 2, map.Height() - 2};
    for (int y = inside.y; y < inside.Bottom(); ++y)
        for (int x = inside.x; x < inside.Right(); ++x)
            map[{x, y}] = rng.Chance(settings.wallChance) ? MapCell::Wall : MapCell::Floor;
    Grid<MapCell> next = map;
    for (int round = 0; round < settings.smoothing; ++round) {
        for (int y = inside.y; y < inside.Bottom(); ++y)
            for (int x = inside.x; x < inside.Right(); ++x) {
                const int walls = WallsAround(map, {x, y});
                const bool wall = walls >= 5 || (walls >= 4 && map[{x, y}] == MapCell::Wall);
                next[{x, y}] = wall ? MapCell::Wall : MapCell::Floor;
            }
        std::swap(map, next);
    }
    FillSmallRegions(map, static_cast<std::size_t>(std::max(settings.minRegion, 0)));
    ConnectRegions(map);
    return map;
}
