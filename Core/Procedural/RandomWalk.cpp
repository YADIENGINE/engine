#include "Procedural/RandomWalk.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include "Utilities/Geometry/Rect.h"

namespace {
constexpr std::array<Vec2, 4> Steps{Vec2{0, -1}, Vec2{-1, 0}, Vec2{1, 0}, Vec2{0, 1}};
constexpr std::int64_t StepsPerCell = 50;
} // namespace

Grid<MapCell> GenerateRandomWalk(const RandomWalkSettings& settings, Rng& rng) {
    Grid<MapCell> map(settings.size, MapCell::Wall);
    if (map.Width() < 3 || map.Height() < 3) return map;
    const Rect inside{1, 1, map.Width() - 2, map.Height() - 2};
    const auto insideCells = static_cast<std::int64_t>(inside.width) * inside.height;
    const float share = std::clamp(settings.floorShare, 0.0f, 1.0f);
    const auto wanted =
        std::clamp(static_cast<std::int64_t>(share * static_cast<float>(insideCells)), std::int64_t{1}, insideCells);

    Vec2 at = settings.start.value_or(Vec2{inside.CenterX(), inside.CenterY()});
    at = {std::clamp(at.x, inside.x, inside.Right() - 1), std::clamp(at.y, inside.y, inside.Bottom() - 1)};
    map[at] = MapCell::Floor;
    std::int64_t floors = 1;
    for (std::int64_t step = 0; floors < wanted && step < insideCells * StepsPerCell; ++step) {
        const Vec2 next = at + Steps[rng.Index(Steps.size())];
        if (!inside.Contains(next)) continue; // Bumps into the border and tries another way
        at = next;
        if (map[at] == MapCell::Wall) {
            map[at] = MapCell::Floor;
            ++floors;
        }
    }
    return map;
}
