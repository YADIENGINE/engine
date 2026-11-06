#include "Procedural/Rooms.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace {
// One part of the map: a leaf holds a room, a cut part two smaller parts.
struct Part {
    Rect area;
    int first = -1; // The two parts it was cut into
    int second = -1;
    int room = -1;
};

// A part holds a room and the wall around it: one cell on each side, so rooms of neighbouring parts are two apart.
constexpr int Margin = 2;

// Cuts 'part' across its longer side (a square one either way) when it is larger than a room needs and both halves
// can still hold the smallest room. Returns false for a part that stays a leaf.
bool Cut(std::vector<Part>& parts, std::size_t index, int smallest, int largest, Rng& rng) {
    const Rect area = parts[index].area;
    const bool wide = area.width > largest && area.width >= 2 * smallest;
    const bool tall = area.height > largest && area.height >= 2 * smallest;
    if (!wide && !tall) return false;
    bool across = wide; // Across the width: a vertical cut
    if (wide && tall) across = area.width == area.height ? rng.Below(2) == 0U : area.width > area.height;
    const int length = across ? area.width : area.height;
    const int at = smallest + rng.Int(0, length - 2 * smallest);
    Rect first = area, second = area;
    if (across) {
        first.width = at;
        second.x += at;
        second.width -= at;
    } else {
        first.height = at;
        second.y += at;
        second.height -= at;
    }
    parts[index].first = static_cast<int>(parts.size());
    parts.push_back({first});
    parts[index].second = static_cast<int>(parts.size());
    parts.push_back({second});
    return true;
}

std::vector<int> RoomsUnder(const std::vector<Part>& parts, int index) {
    std::vector<int> rooms;
    std::vector<int> open{index};
    while (!open.empty()) {
        const Part& part = parts[static_cast<std::size_t>(open.back())];
        open.pop_back();
        if (part.room >= 0) rooms.push_back(part.room);
        if (part.second >= 0) open.push_back(part.second);
        if (part.first >= 0) open.push_back(part.first);
    }
    return rooms;
}

std::int64_t CenterDistance(const Rect& a, const Rect& b) {
    const std::int64_t dx = a.CenterX() - b.CenterX();
    const std::int64_t dy = a.CenterY() - b.CenterY();
    return dx * dx + dy * dy;
}

// A corridor square of 'width' cells at 'cell', kept off the map's border.
void Dig(Grid<MapCell>& map, Vec2 cell, int width) {
    const Rect inside{1, 1, map.Width() - 2, map.Height() - 2};
    const int back = (width - 1) / 2;
    for (int y = cell.y - back; y < cell.y - back + width; ++y)
        for (int x = cell.x - back; x < cell.x - back + width; ++x)
            if (inside.Contains({x, y})) map[{x, y}] = MapCell::Floor;
}

void Corridor(Grid<MapCell>& map, Vec2 from, Vec2 to, int width, Rng& rng) {
    const Vec2 corner = rng.Below(2) == 0U ? Vec2{to.x, from.y} : Vec2{from.x, to.y};
    for (const auto& [a, b] : {std::pair{from, corner}, std::pair{corner, to}}) {
        const Vec2 step{b.x > a.x ? 1 : (b.x < a.x ? -1 : 0), b.y > a.y ? 1 : (b.y < a.y ? -1 : 0)};
        for (Vec2 cell = a;; cell += step) {
            Dig(map, cell, width);
            if (cell == b) break;
        }
    }
}
} // namespace

RoomLayout GenerateRooms(const RoomSettings& settings, Rng& rng) {
    RoomLayout layout{.cells = Grid<MapCell>(settings.size, MapCell::Wall)};
    const int minRoom = std::max(settings.minRoom, 1);
    const int maxRoom = std::max(settings.maxRoom, minRoom);
    const int smallest = minRoom + Margin;
    const int largest = maxRoom + Margin;
    Grid<MapCell>& map = layout.cells;
    if (map.Width() < smallest || map.Height() < smallest) return layout;

    std::vector<Part> parts{{map.Bounds()}};
    for (std::size_t index = 0; index < parts.size(); ++index) {
        if (Cut(parts, index, smallest, largest, rng)) continue;
        Part& leaf = parts[index];
        const int width = rng.Int(minRoom, std::min(maxRoom, leaf.area.width - Margin));
        const int height = rng.Int(minRoom, std::min(maxRoom, leaf.area.height - Margin));
        const Rect room{leaf.area.x + 1 + rng.Int(0, leaf.area.width - Margin - width),
                        leaf.area.y + 1 + rng.Int(0, leaf.area.height - Margin - height), width, height};
        leaf.room = static_cast<int>(layout.rooms.size());
        layout.rooms.push_back(room);
        map.Fill(room, MapCell::Floor);
    }

    // Join the halves of every cut, the last cuts first: each corridor joins the two nearest rooms across one cut.
    const int width = std::max(settings.corridorWidth, 1);
    for (std::size_t index = parts.size(); index-- > 0;) {
        const Part& part = parts[index];
        if (part.first < 0) continue;
        RoomLink nearest;
        std::int64_t best = std::numeric_limits<std::int64_t>::max();
        for (const int a : RoomsUnder(parts, part.first))
            for (const int b : RoomsUnder(parts, part.second)) {
                const std::int64_t distance = CenterDistance(layout.rooms[static_cast<std::size_t>(a)],
                                                             layout.rooms[static_cast<std::size_t>(b)]);
                if (distance < best) {
                    best = distance;
                    nearest = {a, b};
                }
            }
        const Rect& from = layout.rooms[static_cast<std::size_t>(nearest.from)];
        const Rect& to = layout.rooms[static_cast<std::size_t>(nearest.to)];
        Corridor(map, {from.CenterX(), from.CenterY()}, {to.CenterX(), to.CenterY()}, width, rng);
        layout.links.push_back(nearest);
    }
    return layout;
}
