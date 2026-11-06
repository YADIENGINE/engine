#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "Procedural/Caves.h"
#include "Procedural/RandomWalk.h"
#include "Procedural/Regions.h"
#include "Procedural/Rooms.h"

namespace {
bool BorderIsWall(const Grid<MapCell>& map) {
    for (int x = 0; x < map.Width(); ++x)
        if (map[{x, 0}] != MapCell::Wall || map[{x, map.Height() - 1}] != MapCell::Wall) return false;
    for (int y = 0; y < map.Height(); ++y)
        if (map[{0, y}] != MapCell::Wall || map[{map.Width() - 1, y}] != MapCell::Wall) return false;
    return true;
}
bool OneFloorArea(const Grid<MapCell>& map) { return FloorRegions(map).size() == 1; }

// Rows of '#' and '.', top first.
Grid<MapCell> Map(const std::vector<const char*>& rows) {
    Grid<MapCell> map({static_cast<int>(std::char_traits<char>::length(rows.front())), static_cast<int>(rows.size())});
    for (int y = 0; y < map.Height(); ++y)
        for (int x = 0; x < map.Width(); ++x)
            map[{x, y}] = rows[static_cast<std::size_t>(y)][x] == '.' ? MapCell::Floor : MapCell::Wall;
    return map;
}
} // namespace

TEST_CASE("Engine.Rooms.EveryRoomIsReachableAndWalledOff", "[engine][procedural]") {
    for (const std::uint64_t seed : {1ULL, 2ULL, 3ULL, 40ULL}) {
        Rng rng(seed);
        const RoomSettings settings{.size = {64, 40}, .minRoom = 4, .maxRoom = 9};
        const RoomLayout layout = GenerateRooms(settings, rng);
        INFO("seed " << seed);
        REQUIRE(layout.rooms.size() >= 6);
        CHECK(layout.links.size() == layout.rooms.size() - 1);
        CHECK(BorderIsWall(layout.cells));
        CHECK(OneFloorArea(layout.cells));
        for (std::size_t i = 0; i < layout.rooms.size(); ++i) {
            const Rect& room = layout.rooms[i];
            CHECK(room.width >= 4);
            CHECK(room.width <= 9);
            CHECK(room.height >= 4);
            CHECK(room.height <= 9);
            for (std::size_t j = i + 1; j < layout.rooms.size(); ++j) {
                const Rect& other = layout.rooms[j];
                const bool apart = other.x >= room.Right() + 2 || room.x >= other.Right() + 2 ||
                                   other.y >= room.Bottom() + 2 || room.y >= other.Bottom() + 2;
                CHECK(apart);
            }
        }
    }
}
TEST_CASE("Engine.Rooms.TheSameSeedGivesTheSameLayout", "[engine][procedural]") {
    Rng first(77), second(77), other(78);
    const RoomSettings settings{.size = {50, 30}, .corridorWidth = 2};
    const RoomLayout a = GenerateRooms(settings, first);
    const RoomLayout b = GenerateRooms(settings, second);
    CHECK(a.cells == b.cells);
    CHECK(a.rooms.size() == b.rooms.size());
    CHECK_FALSE(a.cells == GenerateRooms(settings, other).cells);
}
TEST_CASE("Engine.Rooms.ATinyMapHasNoRooms", "[engine][procedural]") {
    Rng rng(1);
    const RoomLayout layout = GenerateRooms({.size = {5, 30}, .minRoom = 4}, rng);
    CHECK(layout.rooms.empty());
    CHECK(layout.cells.Count(MapCell::Floor) == 0);
    const RoomLayout single = GenerateRooms({.size = {6, 6}, .minRoom = 4}, rng);
    CHECK(single.rooms.size() == 1);
    CHECK(single.links.empty());
}

TEST_CASE("Engine.RandomWalk.DigsTheShareAskedForInOnePiece", "[engine][procedural]") {
    Rng rng(6);
    const RandomWalkSettings settings{.size = {40, 30}, .floorShare = 0.3f};
    const Grid<MapCell> map = GenerateRandomWalk(settings, rng);
    const std::size_t inside = 38U * 28U;
    CHECK(map.Count(MapCell::Floor) == static_cast<std::size_t>(0.3f * static_cast<float>(inside)));
    CHECK(BorderIsWall(map));
    CHECK(OneFloorArea(map));
    Rng again(6);
    CHECK(map == GenerateRandomWalk(settings, again));
}
TEST_CASE("Engine.RandomWalk.StartsWhereAskedAndStopsOnAFullMap", "[engine][procedural]") {
    Rng rng(2);
    const Grid<MapCell> map = GenerateRandomWalk({.size = {20, 20}, .floorShare = 0.05f, .start = Vec2{2, 3}}, rng);
    CHECK(map[{2, 3}] == MapCell::Floor);
    const Grid<MapCell> full = GenerateRandomWalk({.size = {8, 8}, .floorShare = 5.0f}, rng); // Clamped to all
    CHECK(full.Count(MapCell::Floor) == 36U);
    CHECK(GenerateRandomWalk({.size = {2, 9}}, rng).Count(MapCell::Floor) == 0U);
}

TEST_CASE("Engine.Caves.AreOnePieceAndDiffer", "[engine][procedural]") {
    for (const std::uint64_t seed : {1ULL, 5ULL, 9ULL}) {
        Rng rng(seed);
        const Grid<MapCell> map = GenerateCaves({.size = {80, 45}}, rng);
        INFO("seed " << seed);
        CHECK(BorderIsWall(map));
        CHECK(OneFloorArea(map));
        const std::size_t floors = map.Count(MapCell::Floor);
        CHECK(floors > map.Cells().size() / 4); // Caves, not specks
        CHECK(floors < map.Cells().size() * 3 / 4);
    }
    Rng first(3), second(3), other(4);
    const Grid<MapCell> a = GenerateCaves({}, first);
    CHECK(a == GenerateCaves({}, second));
    CHECK_FALSE(a == GenerateCaves({}, other));
}

TEST_CASE("Engine.Regions.AreFoundLargestFirst", "[engine][procedural]") {
    const auto map = Map({"#######", "#..#.##", "#..#..#", "####.##", "#.#####", "#######"});
    const auto regions = FloorRegions(map);
    REQUIRE(regions.size() == 3);
    CHECK(regions[0].size() == 4);
    CHECK(regions[0].front() == Vec2{1, 1});
    CHECK(regions[1].size() == 4); // Same size, met later in the scan
    CHECK(regions[1].front() == Vec2{4, 1});
    CHECK(regions[2] == std::vector<Vec2>{{1, 4}});
}
TEST_CASE("Engine.Regions.SmallOnesFillAndTheRestJoin", "[engine][procedural]") {
    auto map = Map({"##########", "#..####..#", "#..####..#", "##########", "#.######.#", "##########"});
    FillSmallRegions(map, 2);
    CHECK(map[{1, 4}] == MapCell::Wall);
    CHECK(map[{8, 4}] == MapCell::Wall);
    CHECK(FloorRegions(map).size() == 2);
    ConnectRegions(map);
    CHECK(OneFloorArea(map));
    CHECK(map.Count(MapCell::Floor) == 8U + 4U); // A shortest tunnel through the four walls between
    CHECK(BorderIsWall(map));
    const auto joined = map;
    ConnectRegions(map); // Nothing left to join
    CHECK(map == joined);
}
