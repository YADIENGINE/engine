#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "Audio/SoundPlayer.h"
#include "Audio/SoundTables.h"
#include "Runtime/GameSettings.h"
#include "Tests/Support/FakeSoundOutput.h"

namespace {
enum class Sfx { Hit, Coin, UiMove, Count };
enum class Voice { Hello, Count };
enum class Stranger { Nobody, Count };
enum class Plain { One };

inline constexpr std::array SfxTable{
    SoundRow<Sfx>{Sfx::Hit,
                  {"Audio/hit1.wav", "Audio/hit2.wav", "Audio/hit3.wav"},
                  {.pitch = {0.5f, 1.5f}, .cooldown = 0.25f}},
    SoundRow<Sfx>{Sfx::Coin,
                  {"Audio/coin.wav"},
                  {.volume = 0.5f, .whenPlaying = SoundOverlap::Copies(2), .label = "Coin"}},
    SoundRow<Sfx>{Sfx::UiMove, {"Audio/move1.wav", "Audio/move2.wav"}, {.whenPlaying = SoundOverlap::Skip}},
};
static_assert(RowsInIdOrder(SfxTable));
static_assert(SfxTable[0].files[2] == "Audio/hit3.wav" && SfxTable[0].options.cooldown == 0.25f);
static_assert(SfxTable[2].options.whenPlaying.IsSkip() && SfxTable[2].options.whenPlaying.Voices() == 1);
static_assert(SfxTable[1].options.whenPlaying.Voices() == 2);

// The class template argument comes from the id too.
inline constexpr std::array VoiceTable{SoundRow{Voice::Hello, {"Audio/hello.wav"}}};
static_assert(RowsInIdOrder(VoiceTable));

// A row that is invalid does not compile: this asks the compiler whether making the row is a constant expression.
template <class Make> concept CompilesAsConstant = requires { typename std::bool_constant<(Make{}(), true)>; };

static_assert(CompilesAsConstant<decltype([] { return SoundRow{Sfx::Hit, {"a.wav"}}; })>);
static_assert(!CompilesAsConstant<decltype([] { return SoundRow{Sfx::Hit, {}}; })>);
static_assert(!CompilesAsConstant<decltype([] { return SoundRow{Sfx::Hit, {"/a.wav"}}; })>);
static_assert(!CompilesAsConstant<decltype([] { return SoundRow{Sfx::Hit, {"C:/a.wav"}}; })>);
static_assert(!CompilesAsConstant<decltype([] { return SoundRow{Sfx::Hit, {"Audio/../../a.wav"}}; })>);
static_assert(!CompilesAsConstant<decltype([] { return SoundRow{Sfx::Hit, {"a.wav", "", "b.wav"}}; })>);
static_assert(!CompilesAsConstant<decltype([] { return SoundRow{Sfx::Hit, {"a.wav", "a.wav"}}; })>);
static_assert(!CompilesAsConstant<decltype([] { return SoundRow{Sfx::Hit, {"a.wav"}, {.volume = 1.5f}}; })>);
static_assert(!CompilesAsConstant<decltype([] { return SoundRow{Sfx::Hit, {"a.wav"}, {.pitch = {0.0f, 1.0f}}}; })>);
static_assert(!CompilesAsConstant<decltype([] { return SoundRow{Sfx::Hit, {"a.wav"}, {.pitch = {2.0f, 1.0f}}}; })>);
static_assert(!CompilesAsConstant<decltype([] { return SoundRow{Sfx::Hit, {"a.wav"}, {.cooldown = -1.0f}}; })>);
static_assert(!CompilesAsConstant<decltype([] { return SoundOverlap::Copies(0); })>);

// A table that misses a row cannot become tables; an enum without Count (SoundId, a catalog ID) cannot become a key.
static_assert(std::is_convertible_v<decltype(SfxTable), SoundTables>);
static_assert(!std::is_convertible_v<std::array<SoundRow<Sfx>, 2>, SoundTables>);
static_assert(std::is_convertible_v<Sfx, SoundKey>);
static_assert(!std::is_convertible_v<SoundId, SoundKey>);
static_assert(!std::is_convertible_v<Plain, SoundKey>);
static_assert(!std::is_convertible_v<int, SoundKey>);

class FakeOutput final : public FakeSoundOutput {
public:
    [[nodiscard]] VoiceId StartVoice(SoundId id, const VoiceStart& start) override {
        plays.emplace_back(id, start.pitch);
        return FakeSoundOutput::StartVoice(id, start);
    }
    [[nodiscard]] std::size_t File(std::size_t play, SoundId first) const {
        return static_cast<std::size_t>(plays[play].first) - static_cast<std::size_t>(first);
    }
    std::vector<std::pair<SoundId, float>> plays;
};

SoundId First(const SoundTables& tables, std::size_t table, std::size_t row) {
    return tables.Tables()[table].rows[row].firstId;
}
} // namespace

TEST_CASE("Engine.Sounds.RowThrowsOutsideAConstantToo", "[engine][audio]") {
    // The same check as the compile-time one, for a table made while the game runs (one loaded from text, later).
    const auto make = [](float volume) { return SoundRow{Sfx::Hit, {"a.wav"}, {.volume = volume}}; };
    CHECK_NOTHROW(make(1.0f));
    CHECK_THROWS_AS(make(1.5f), std::invalid_argument);
    CHECK_THROWS_AS(make(std::nanf("")), std::invalid_argument);
    CHECK_THROWS_AS(SoundRow(Sfx::Hit, {}), std::invalid_argument);
    CHECK_THROWS_AS(SoundOverlap::Copies(0), std::invalid_argument);
    const auto infinite = std::numeric_limits<float>::infinity();
    CHECK_THROWS_AS(SoundRow(Sfx::Hit, {"a.wav"}, {.pitch = {1.0f, infinite}}), std::invalid_argument);
    CHECK_THROWS_AS(SoundRow(Sfx::Hit, {"a.wav"}, {.cooldown = infinite}), std::invalid_argument);
}

TEST_CASE("Engine.Sounds.PathsFollowTheCatalogRules", "[engine][audio]") {
    CHECK(SoundFileInsideAssets("a.wav"));
    CHECK(SoundFileInsideAssets("Audio/..a/b..wav"));
    CHECK_FALSE(SoundFileInsideAssets(""));
    CHECK_FALSE(SoundFileInsideAssets("\\a.wav"));
    CHECK_FALSE(SoundFileInsideAssets("Audio\\..\\a.wav"));
    CHECK_FALSE(SoundFileInsideAssets(".."));
}

TEST_CASE("Engine.Sounds.TablesCatchWhatACompilerWasNotAskedAbout", "[engine][audio]") {
    // Out of order, without the static_assert.
    const std::array swapped{SoundRow{Voice::Count, {"a.wav"}}};
    CHECK_THROWS_WITH(SoundTables(swapped), Catch::Matchers::ContainsSubstring("Voice"));
    // A row changed after it was checked.
    std::array changed{SoundRow{Voice::Hello, {"a.wav"}}};
    changed[0].options.pitch = {2.0f, 1.0f};
    CHECK_THROWS_AS(SoundTables(changed), std::invalid_argument);
    // One table per enum.
    CHECK_THROWS_WITH((SoundTables{SfxTable, SfxTable}),
                      Catch::Matchers::ContainsSubstring("Two sound tables use the enum Sfx"));
}

TEST_CASE("Engine.Sounds.AssignsLikeTheDocsSay", "[engine][audio]") {
    GameSettings settings;
    CHECK(settings.sounds.Tables().empty());
    settings.sounds = SfxTable;
    CHECK(settings.sounds.Tables().size() == 1);
    settings.sounds = {SfxTable, VoiceTable};
    CHECK(settings.sounds.Tables().size() == 2);
    settings.sounds = {};
    CHECK(settings.sounds.Tables().empty());
}

TEST_CASE("Engine.Sounds.CatalogGainsAnEntryPerFileWithLabels", "[engine][audio]") {
    const AudioCatalog own{.effects = {{SoundId{0}, "Audio/own.wav", {}, "Own"}},
                           .music = {{MusicId{0}, "Audio/menu.ogg"}}};
    const AudioCatalog full = CatalogWithSounds(own, {SfxTable, VoiceTable});
    REQUIRE(full.effects.size() == 8);
    CHECK(full.music.size() == 1);
    CHECK(full.effects[0].name == "Own");
    CHECK(static_cast<std::size_t>(full.effects[1].id) == SoundTables::FirstGeneratedId);
    CHECK(full.effects[1].name == "Sfx[0] (1/3)");
    CHECK(full.effects[3].name == "Sfx[0] (3/3)");
    CHECK(full.effects[3].file == "Audio/hit3.wav");
    CHECK(full.effects[4].name == "Coin");
    CHECK(full.effects[4].settings.volume == 0.5f);
    CHECK(full.effects[4].settings.maxConcurrent == 2);
    CHECK(full.effects[4].settings.overflow == EffectOverflow::RestartNext);
    CHECK(full.effects[5].settings.maxConcurrent == 1);
    CHECK(full.effects[5].settings.overflow == EffectOverflow::Skip);
    CHECK(full.effects[7].name == "Voice[0]");
    CHECK(static_cast<std::size_t>(full.effects[7].id) == SoundTables::FirstGeneratedId + 6);
}

TEST_CASE("Engine.Sounds.GameIdsStayClearOfGeneratedOnes", "[engine][audio]") {
    const AudioCatalog clash{.effects = {{SoundId{SoundTables::FirstGeneratedId}, "Audio/own.wav"}}};
    CHECK_THROWS_WITH(CatalogWithSounds(clash, SfxTable), Catch::Matchers::ContainsSubstring("reserved"));
    const AudioCatalog sparse{.effects = {{SoundId{SoundTables::FirstGeneratedId - 1}, "Audio/own.wav"}}};
    CHECK_NOTHROW(CatalogWithSounds(sparse, SfxTable));
    const AudioCatalog invalid{.effects = {{SoundId{1}, "/absolute.wav"}}};
    CHECK_THROWS_AS(CatalogWithSounds(invalid, {}), std::invalid_argument);
}

TEST_CASE("Engine.Sounds.HasAsksTheTablesNotTheDevice", "[engine][audio]") {
    FakeOutput output;
    const SoundPlayer player(SfxTable, output);
    CHECK(player.Has(Sfx::Hit));
    CHECK(player.Has(SoundKey(Sfx::UiMove)));
    CHECK_FALSE(player.Has(Sfx::Count));
    CHECK_FALSE(player.Has(Stranger::Nobody));
    CHECK_FALSE(player.Has(SoundKey{}));
}

TEST_CASE("Engine.Sounds.WrongKeysThrowInEveryConfiguration", "[engine][audio]") {
    FakeOutput output;
    SoundPlayer player(SfxTable, output);
    CHECK_THROWS_AS(player.Play(Stranger::Nobody), std::out_of_range);
    CHECK_THROWS_WITH(player.Play(Stranger::Nobody),
                      "The sound enum Stranger was never registered: add its table to GameSettings::sounds");
    CHECK_THROWS_WITH(player.Play(Sfx::Count), "The sound enum Sfx has no row 3");
    CHECK_THROWS_AS(player.Play(SoundKey{}), std::out_of_range);
    CHECK(output.plays.empty());
}

TEST_CASE("Engine.Sounds.BadPitchThrowsEvenDuringACooldown", "[engine][audio]") {
    FakeOutput output;
    SoundPlayer player(SfxTable, output);
    CHECK(player.Play(Sfx::Hit));
    CHECK_THROWS_AS(player.Play(Sfx::Hit, {.pitch = 0.0f}), std::invalid_argument);
    CHECK_THROWS_AS(player.Play(Sfx::Hit, {.pitch = std::nanf("")}), std::invalid_argument);
}

TEST_CASE("Engine.Sounds.NeverTheSameFileTwiceInARowAndEveryFileComes", "[engine][audio]") {
    const SoundTables tables = SfxTable;
    FakeOutput output;
    SoundPlayer player(tables, output);
    const SoundId first = First(tables, 0, 0);
    std::map<std::size_t, int> counts;
    for (int i = 0; i < 3000; ++i) {
        REQUIRE(player.Play(Sfx::Hit));
        player.Update(1.0f);
        const std::size_t file = output.File(output.plays.size() - 1, first);
        REQUIRE(file < 3);
        if (i > 0) REQUIRE(file != output.File(output.plays.size() - 2, first));
        ++counts[file];
    }
    for (std::size_t file = 0; file < 3; ++file) // Evenly, give or take
        CHECK(std::abs(counts[file] - 1000) < 100);
}

TEST_CASE("Engine.Sounds.SameSeedPicksTheSame", "[engine][audio]") {
    const auto run = [](std::uint64_t seed) {
        FakeOutput output;
        SoundPlayer player(SfxTable, output, seed);
        for (int i = 0; i < 20; ++i) {
            player.Play(Sfx::Hit);
            player.Update(1.0f);
        }
        return output.plays;
    };
    CHECK(run(SoundPlayer::DefaultSeed) == run(SoundPlayer::DefaultSeed));
    CHECK(run(1) != run(2));
}

TEST_CASE("Engine.Sounds.PitchIsPickedFromTheRangeTimesTheCaller", "[engine][audio]") {
    FakeOutput output;
    SoundPlayer player(SfxTable, output);
    float lowest = 100.0f;
    float highest = 0.0f;
    for (int i = 0; i < 500; ++i) {
        player.Play(Sfx::Hit, {.pitch = 2.0f});
        player.Update(1.0f);
        lowest = std::min(lowest, output.plays.back().second);
        highest = std::max(highest, output.plays.back().second);
    }
    CHECK(lowest >= 1.0f);
    CHECK(highest < 3.0f);
    CHECK(lowest < 1.1f);
    CHECK(highest > 2.9f);
    player.Play(Sfx::Coin); // No range: exactly 1
    CHECK(output.plays.back().second == 1.0f);
    player.Play(Sfx::Coin, {.pitch = 0.75f});
    CHECK(output.plays.back().second == 0.75f);
}

TEST_CASE("Engine.Sounds.CooldownCountsRealSeconds", "[engine][audio]") {
    FakeOutput output;
    SoundPlayer player(SfxTable, output);
    CHECK(player.Play(Sfx::Hit));
    CHECK_FALSE(player.Play(Sfx::Hit)); // Same frame
    player.Update(0.125f);
    CHECK_FALSE(player.Play(Sfx::Hit));
    player.Update(0.125f);
    CHECK(player.Play(Sfx::Hit)); // Exactly the cooldown has passed
    CHECK(output.plays.size() == 2);
    // Another row has its own, or none.
    CHECK(player.Play(Sfx::Coin));
    CHECK(player.Play(Sfx::Coin));
}

TEST_CASE("Engine.Sounds.SkipLooksAtEveryFileOfTheRow", "[engine][audio]") {
    const SoundTables tables = SfxTable;
    FakeOutput output;
    SoundPlayer player(tables, output);
    const SoundId first = First(tables, 0, 2);
    CHECK(player.Play(Sfx::UiMove));
    const std::size_t played = output.File(0, first);
    output.playing.insert(output.plays[0].first);
    CHECK_FALSE(player.Play(Sfx::UiMove));
    output.playing.clear();
    output.playing.insert(SoundId{static_cast<std::size_t>(first) + (1 - played)}); // The row's other file
    CHECK_FALSE(player.Play(Sfx::UiMove));
    CHECK(output.plays.size() == 1);
    output.playing.clear();
    CHECK(player.Play(Sfx::UiMove));
    CHECK(output.File(1, first) != played); // A refused play did not count as the last file
}

TEST_CASE("Engine.Sounds.ARefusedPlayLeavesTheCooldownAlone", "[engine][audio]") {
    enum class Row { Slow, Count };
    static constexpr std::array table{
        SoundRow{Row::Slow, {"a.wav"}, {.cooldown = 1.0f, .whenPlaying = SoundOverlap::Skip}}};
    FakeOutput output;
    const SoundTables tables = table;
    SoundPlayer player(tables, output);
    output.playing.insert(First(tables, 0, 0));
    CHECK_FALSE(player.Play(Row::Slow)); // Skip, not a real play
    output.playing.clear();
    CHECK(player.Play(Row::Slow)); // So there is no cooldown to wait for
}

TEST_CASE("Engine.Sounds.TablesAreSeparateByEnum", "[engine][audio]") {
    const SoundTables tables{SfxTable, VoiceTable};
    FakeOutput output;
    SoundPlayer player(tables, output);
    CHECK(player.Play(Voice::Hello));
    CHECK(output.plays.back().first == First(tables, 1, 0));
    CHECK(player.Has(Sfx::Coin));
    CHECK(static_cast<std::size_t>(First(tables, 1, 0)) > static_cast<std::size_t>(First(tables, 0, 2)));
}

TEST_CASE("Engine.Sounds.AKeyStoresWithoutTheEnum", "[engine][audio]") {
    struct OtherSystem {
        SoundKey move;
        SoundKey silent;
    };
    const OtherSystem system{.move = Sfx::UiMove};
    FakeOutput output;
    SoundPlayer player(SfxTable, output);
    CHECK(system.move);
    CHECK_FALSE(system.silent);
    CHECK(player.Has(system.move));
    CHECK(player.Play(system.move));
}

TEST_CASE("Engine.Sounds.NamesReadPlainly", "[engine][audio]") {
    CHECK(SoundEnumName(typeid(Sfx)) == "Sfx");
}
