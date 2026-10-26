#include "Tests/Support/Diagnostics.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include "Modules/Version/Version.hpp"
#include "Options/GameOptions.h"
#include "Tests/Support/TempDirectory.h"

TEST_CASE("Version.Check.FirstLaunchAndUnchangedStaySilent", "[version]") {
    TempDirectory temp;
    const auto path = temp.Path() / "settings.json";
    GameOptions options(path);
    int calls = 0;
    const auto notify = [&](const Version::Change&) { ++calls; };
    Version::Check(options, {1, 2, 3}, notify);
    CHECK(calls == 0);
    GameOptions loaded(path);
    REQUIRE(loaded.Load() == OptionsLoadResult::Loaded);
    CHECK(loaded.GetInt("app.last_version", -1) == 1'002'003);

    // Same-version checks must not flush unrelated pending options.
    loaded.SetInt("game.score", 42);
    Version::Check(loaded, {1, 2, 3}, notify);
    CHECK(calls == 0);
    CHECK(loaded.Dirty());
    GameOptions unchanged(path);
    (void)unchanged.Load();
    CHECK_FALSE(unchanged.Contains("game.score"));
}

TEST_CASE("Version.Check.ChangesPersistBeforeNotificationAndDoNotRepeat", "[version]") {
    TempDirectory temp;
    const auto path = temp.Path() / "settings.json";
    GameOptions options(path);
    // Existing installations keep the same key and integer encoding.
    options.SetInt("app.last_version", 1'002'003);
    options.SetInt("game.score", 42);
    int calls = 0;
    Version::Check(options, {2, 0, 0}, [&](const Version::Change& change) {
        ++calls;
        CHECK(change.kind == Version::ChangeKind::Upgrade);
        CHECK(change.previous == GameVersion{1, 2, 3});
        CHECK(change.current == GameVersion{2, 0, 0});
        GameOptions persisted(path);
        (void)persisted.Load();
        CHECK(persisted.GetInt("app.last_version") == 2'000'000);
        CHECK(persisted.GetInt("game.score") == 42);
    });
    REQUIRE(calls == 1);
    GameOptions loaded(path);
    (void)loaded.Load();
    Version::Check(loaded, {2, 0, 0}, [&](const Version::Change&) { ++calls; });
    CHECK(calls == 1);
    Version::Check(loaded, {1, 2, 3}, [&](const Version::Change& change) {
        ++calls;
        CHECK(change.kind == Version::ChangeKind::Downgrade);
        CHECK(change.previous == GameVersion{2, 0, 0});
        CHECK(change.current == GameVersion{1, 2, 3});
    });
    CHECK(calls == 2);
    GameOptions downgraded(path);
    (void)downgraded.Load();
    CHECK(downgraded.GetInt("app.last_version") == 1'002'003);
}

TEST_CASE("Version.Check.InvalidHistoryIsFirstLaunchAndInvalidCurrentDoesNoWork", "[version]") {
    TempDirectory temp;
    const auto path = temp.Path() / "settings.json";
    GameOptions options(path);
    int calls = 0;
    const auto notify = [&](const Version::Change&) { ++calls; };
    for (const auto invalid : std::array{GameVersion{-1, 0, 0}, GameVersion{1, 1000, 0}, GameVersion{1, 0, 1000}})
        CHECK_THROWS_AS(Version::Check(options, invalid, notify), std::invalid_argument);
    CHECK_FALSE(options.Dirty());
    CHECK_FALSE(std::filesystem::exists(path));
    for (const auto invalid : {-5LL, std::numeric_limits<long long>::max()}) {
        options.SetInt("app.last_version", invalid);
        Version::Check(options, {1, 0, 0}, notify);
        CHECK(calls == 0);
        CHECK(options.GetInt("app.last_version") == 1'000'000);
    }
}

TEST_CASE("Version.Check.SaveFailureSkipsNotificationAndAllowsRetry", "[version]") {
    TempDirectory temp;
    const auto blocker = temp.Path() / "blocked";
    std::ofstream(blocker) << "not a directory";
    GameOptions options(blocker / "settings.json");
    options.SetInt("app.last_version", 1'000'000);
    int calls = 0;
    const auto notify = [&](const Version::Change&) { ++calls; };
    CHECK_THROWS(Version::Check(options, {2, 0, 0}, notify));
    CHECK(calls == 0);
    CHECK(options.GetInt("app.last_version") == 1'000'000);
    REQUIRE(std::filesystem::remove(blocker));
    Version::Check(options, {2, 0, 0}, notify);
    CHECK(calls == 1);
    CHECK(options.GetInt("app.last_version") == 2'000'000);
}

TEST_CASE("Version.Check.CallbackFailureLeavesVersionRecorded", "[version]") {
    TempDirectory temp;
    const auto path = temp.Path() / "settings.json";
    GameOptions options(path);
    Version::Check(options, {1, 0, 0});
    CHECK_THROWS_AS(Version::Check(options, {2, 0, 0},
                                   [](const Version::Change&) { throw std::runtime_error("Game response failed"); }),
                    std::runtime_error);
    GameOptions loaded(path);
    (void)loaded.Load();
    int calls = 0;
    Version::Check(loaded, {2, 0, 0}, [&](const Version::Change&) { ++calls; });
    CHECK(calls == 0);
}

TEST_CASE("Version.Notes.ValidateAndSelectNewerVersions", "[version]") {
    constexpr std::array notes{PatchNote{{1, 2, 0}, "b", ""}, PatchNote{{1, 1, 5}, "a", ""}};
    static_assert(Version::PatchNotesValid(notes));
    constexpr std::array unordered{PatchNote{{1, 1, 0}, "", ""}, PatchNote{{1, 2, 0}, "", ""}};
    static_assert(!Version::PatchNotesValid(unordered));
    constexpr std::array duplicates{notes[0], notes[0]};
    static_assert(!Version::PatchNotesValid(duplicates));
    static_assert(!Version::IsValid({1, 1000, 0}));
    static_assert(Version::IsValid({std::numeric_limits<int>::max(), 999, 999}));
    CHECK(Version::NotesSince(notes, {1, 1, 5}).size() == 1);
    CHECK(Version::NotesSince(notes, {0, 0, 0}).size() == 2);
    CHECK(Version::NotesSince(notes, {2, 0, 0}).empty());
    CHECK(Version::NotesSince({}, {0, 0, 0}).empty());
    CHECK(Version::Text({1, 2, 3}) == "1.2.3");
    CHECK(GameVersion{1, 2, 3} < GameVersion{1, 10, 0});
    CHECK(GameVersion{1, 9, 9} < GameVersion{2, 0, 0});
}
