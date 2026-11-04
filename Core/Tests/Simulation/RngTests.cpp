#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "Utilities/Random/Rng.h"
#include "Utilities/Random/WeightedPick.h"

TEST_CASE("Engine.Rng.FollowsThePublishedPcg32Sequence", "[engine][simulation]") {
    Rng rng(42, 54); // The reference values of the PCG32 demo program
    constexpr std::array<std::uint32_t, 6> expected{0xa15c02b7, 0x7b47f409, 0xba1d3330,
                                                    0x83d2f293, 0xbfa4784b, 0xcbed606e};
    for (const auto value : expected)
        CHECK(rng.Next() == value);
}
TEST_CASE("Engine.Rng.SameSeedRepeatsAndStreamsDiffer", "[engine][simulation]") {
    Rng first(7), second(7), other(7, 1);
    CHECK(first == second);
    CHECK_FALSE(first == other);
    for (int i = 0; i < 100; ++i)
        CHECK(first.Next() == second.Next());
    CHECK(first.Next() != other.Next());
}
TEST_CASE("Engine.Rng.ValuesStayInRange", "[engine][simulation]") {
    Rng rng(99);
    std::array<int, 5> seen{};
    for (int i = 0; i < 5000; ++i) {
        const int value = rng.Int(-2, 2);
        REQUIRE(value >= -2);
        REQUIRE(value <= 2);
        ++seen[static_cast<std::size_t>(value + 2)];
        const float unit = rng.Float01();
        REQUIRE(unit >= 0.0f);
        REQUIRE(unit < 1.0f);
        const double fine = rng.Double01();
        REQUIRE(fine >= 0.0);
        REQUIRE(fine < 1.0);
        REQUIRE(rng.Index(3) < 3);
    }
    for (const int count : seen)
        CHECK(count > 800); // About 1000 each
    CHECK(rng.Int(5, 5) == 5);
}
TEST_CASE("Engine.Rng.ShuffleKeepsTheItems", "[engine][simulation]") {
    Rng rng(3);
    std::vector<int> items{1, 2, 3, 4, 5, 6, 7, 8};
    rng.Shuffle(items);
    CHECK(items != std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8});
    std::vector<int> sorted = items;
    std::ranges::sort(sorted);
    CHECK(sorted == std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8});
}
TEST_CASE("Engine.WeightedIndex.FollowsTheWeights", "[engine][simulation]") {
    Rng rng(11);
    std::array<int, 3> seen{};
    for (int i = 0; i < 4000; ++i)
        ++seen[WeightedIndex(std::array{1.0, 0.0, 3.0}, rng)];
    CHECK(seen[1] == 0);
    CHECK(seen[0] > 800);
    CHECK(seen[0] < 1200);
    CHECK(seen[2] > 2800);
}
