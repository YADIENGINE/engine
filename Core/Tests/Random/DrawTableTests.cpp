#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstddef>
#include <vector>
#include "Utilities/Random/DrawTable.h"

namespace {
std::vector<std::size_t> DrawMany(const DrawTable& table, DrawMemory& memory, Rng& rng, int count) {
    std::vector<std::size_t> drawn;
    for (int i = 0; i < count; ++i)
        drawn.push_back(table.Draw(memory, rng));
    return drawn;
}
} // namespace

TEST_CASE("Engine.DrawTable.FollowsTheWeights", "[engine][random]") {
    const DrawTable table{.entries = {{.weight = 1.0}, {.weight = 3.0}, {.weight = 0.0}}};
    DrawMemory memory;
    Rng rng(5);
    std::array<int, 3> seen{};
    for (const auto entry : DrawMany(table, memory, rng, 4000))
        ++seen[entry];
    CHECK(seen[2] == 0);
    CHECK(seen[0] > 800);
    CHECK(seen[0] < 1200);
    CHECK(seen[1] > 2800);
}
TEST_CASE("Engine.DrawTable.TheSameMemoryAndGeneratorRepeat", "[engine][random]") {
    const DrawTable table{.entries = {{.weight = 5.0}, {.weight = 2.0}, {.weight = 1.0, .pity = 6}}, .noRepeat = true};
    DrawMemory firstMemory, secondMemory;
    Rng first(9), second(9);
    CHECK(DrawMany(table, firstMemory, first, 200) == DrawMany(table, secondMemory, second, 200));
    // Halfway, a copy of both carries on the same way: the memory and the generator are the whole state.
    DrawMemory copiedMemory = firstMemory;
    Rng copied = first;
    CHECK(DrawMany(table, firstMemory, first, 50) == DrawMany(table, copiedMemory, copied, 50));
}
TEST_CASE("Engine.DrawTable.NoRepeatNeverDrawsTheSameEntryTwiceInARow", "[engine][random]") {
    const DrawTable table{.entries = {{.weight = 100.0}, {.weight = 1.0}}, .noRepeat = true};
    DrawMemory memory;
    Rng rng(3);
    const auto drawn = DrawMany(table, memory, rng, 100);
    for (std::size_t i = 1; i < drawn.size(); ++i)
        REQUIRE(drawn[i] != drawn[i - 1]);
    // With one entry that can come up, it repeats rather than fail.
    const DrawTable single{.entries = {{.weight = 1.0}, {.weight = 0.0}}, .noRepeat = true};
    DrawMemory singleMemory;
    CHECK(DrawMany(single, singleMemory, rng, 3) == std::vector<std::size_t>{0, 0, 0});
}
TEST_CASE("Engine.DrawTable.PityForcesTheEntryAfterThatManyMisses", "[engine][random]") {
    // The rare entry has no weight, so only pity brings it: on the 5th draw, then every 5th.
    const DrawTable table{.entries = {{.weight = 1.0}, {.weight = 0.0, .pity = 4}}};
    DrawMemory memory;
    Rng rng(1);
    const auto drawn = DrawMany(table, memory, rng, 10);
    CHECK(drawn == std::vector<std::size_t>{0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
    CHECK(memory.sinceDrawn == std::vector<int>{1, 0});
    CHECK(memory.last == 1);
}
TEST_CASE("Engine.DrawTable.TheMostOverdueEntryWins", "[engine][random]") {
    const DrawTable table{.entries = {{.weight = 1.0}, {.weight = 0.0, .pity = 3}, {.weight = 0.0, .pity = 2}}};
    DrawMemory memory{.sinceDrawn = {0, 5, 5}, .last = 0};
    Rng rng(1);
    CHECK(table.Draw(memory, rng) == 2); // Three past its pity, the other two past
    CHECK(table.Draw(memory, rng) == 1);
}
TEST_CASE("Engine.DrawTable.EmptyOrWeightlessTablesDoNoHarm", "[engine][random]") {
    Rng rng(1);
    DrawMemory memory;
    CHECK(DrawTable{}.Draw(memory, rng) == 0);
    CHECK(memory.sinceDrawn.empty());
    const DrawTable weightless{.entries = {{.weight = 0.0}, {.weight = -2.0}}};
    CHECK(weightless.Draw(memory, rng) == 0);
    // A table that grows keeps the counters it had.
    const DrawTable grown{.entries = {{.weight = 0.0}, {.weight = -2.0}, {.weight = 1.0}}};
    CHECK(grown.Draw(memory, rng) == 2);
    CHECK(memory.sinceDrawn == std::vector<int>{1, 2, 0});
}
