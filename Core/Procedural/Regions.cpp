#include "Procedural/Regions.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <optional>

namespace {
// Up, left, right, down: a fixed order, so floods and tunnels are a function of the map alone.
constexpr std::array<Vec2, 4> Sides{Vec2{0, -1}, Vec2{-1, 0}, Vec2{1, 0}, Vec2{0, 1}};

bool OnBorder(const Grid<MapCell>& map, Vec2 cell) {
    return cell.x == 0 || cell.y == 0 || cell.x == map.Width() - 1 || cell.y == map.Height() - 1;
}

// The floor cells 'from' reaches, in the order a flood fill reaches them; 'seen' marks them.
std::vector<Vec2> Flood(const Grid<MapCell>& map, Vec2 from, Grid<std::uint8_t>& seen) {
    std::vector<Vec2> cells{from};
    seen[from] = 1;
    for (std::size_t next = 0; next < cells.size(); ++next)
        for (const Vec2 side : Sides) {
            const Vec2 cell = cells[next] + side;
            if (!map.Contains(cell) || seen[cell] != 0 || map[cell] != MapCell::Floor) continue;
            seen[cell] = 1;
            cells.push_back(cell);
        }
    return cells;
}
} // namespace

std::vector<std::vector<Vec2>> FloorRegions(const Grid<MapCell>& map) {
    std::vector<std::vector<Vec2>> regions;
    Grid<std::uint8_t> seen(map.Size(), 0);
    for (int y = 0; y < map.Height(); ++y)
        for (int x = 0; x < map.Width(); ++x)
            if (map[{x, y}] == MapCell::Floor && seen[{x, y}] == 0) regions.push_back(Flood(map, {x, y}, seen));
    std::ranges::stable_sort(regions, [](const auto& a, const auto& b) { return a.size() > b.size(); });
    return regions;
}

void FillSmallRegions(Grid<MapCell>& map, std::size_t minCells) {
    for (const auto& region : FloorRegions(map))
        if (region.size() < minCells)
            for (const Vec2 cell : region)
                map[cell] = MapCell::Wall;
}

void ConnectRegions(Grid<MapCell>& map) {
    const auto regions = FloorRegions(map);
    if (regions.size() < 2) return;
    const Vec2 home = regions.front().front();
    const std::size_t cellCount = map.Cells().size();
    for (std::size_t tunnel = 1; tunnel < regions.size(); ++tunnel) {
        // What the largest area reaches now: earlier tunnels may have joined more than one area.
        Grid<std::uint8_t> joined(map.Size(), 0);
        const std::vector<Vec2> reached = Flood(map, home, joined);
        // A breadth-first search from all of it at once, through wall, stops at the first floor cell outside it.
        Grid<int> cameFrom(map.Size(), -1);
        Grid<std::uint8_t> visited = joined;
        std::deque<Vec2> open(reached.begin(), reached.end());
        std::optional<Vec2> found;
        while (!open.empty() && !found) {
            const Vec2 at = open.front();
            open.pop_front();
            for (const Vec2 side : Sides) {
                const Vec2 cell = at + side;
                if (!map.Contains(cell) || visited[cell] != 0) continue;
                const bool floor = map[cell] == MapCell::Floor;
                if (!floor && OnBorder(map, cell)) continue;
                visited[cell] = 1;
                cameFrom[cell] = at.y * map.Width() + at.x;
                if (floor) {
                    found = cell;
                    break;
                }
                open.push_back(cell);
            }
        }
        if (!found) return; // Every area is joined already (or none can be reached)
        for (int index = cameFrom[*found]; index >= 0 && static_cast<std::size_t>(index) < cellCount;) {
            const Vec2 cell{index % map.Width(), index / map.Width()};
            if (joined[cell] != 0) break; // Back at the largest area
            map[cell] = MapCell::Floor;
            index = cameFrom[cell];
        }
    }
}
