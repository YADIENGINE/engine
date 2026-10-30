#include "Tests/Support/Diagnostics.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "Options/GameOptions.h"
#include "Tests/Support/TempDirectory.h"

TEST_CASE("Engine.Options.RoundTripWritesOnlyChanges", "[engine][options]") {
    TempDirectory temp;
    const auto path = temp.Path() / "settings.json";
    GameOptions options(path);
    CHECK(options.Load() == OptionsLoadResult::Missing);
    CHECK_FALSE(options.Save());
    CHECK_FALSE(std::filesystem::exists(path));

    options.SetInt("game.difficulty", 2);
    options.Set(EngineOption::FpsLimit, 120);
    CHECK(options.Dirty());
    CHECK(options.Save());
    CHECK_FALSE(options.Dirty());
    options.SetInt("game.difficulty", 2);
    options.Set(EngineOption::FpsLimit, 120);
    CHECK_FALSE(options.Dirty());

    GameOptions restored(path);
    CHECK(restored.Load() == OptionsLoadResult::Loaded);
    CHECK(restored.LoadResult() == OptionsLoadResult::Loaded);
    CHECK(restored.GetInt("game.difficulty") == 2);
    CHECK(restored.Get(EngineOption::FpsLimit, 60) == 120);
    restored.Erase("game.difficulty");
    CHECK(restored.Dirty());
    CHECK_FALSE(restored.Contains("game.difficulty"));
}

TEST_CASE("Engine.Options.EngineNamespacesClosedToGameKeys", "[engine][options]") {
    TempDirectory temp;
    GameOptions options(temp.Path() / "settings.json");
    options.Set(EngineOption::EffectsVolume, 50);
    CHECK_THROWS_AS(options.SetInt("audio.effects_percent", 0), std::invalid_argument);
    CHECK_THROWS_AS(options.SetInt("audio.custom", 0), std::invalid_argument);
    CHECK_THROWS_AS(options.Erase("display.fps_limit"), std::invalid_argument);
    CHECK(options.Get(EngineOption::EffectsVolume, 100) == 50);
    options.SetInt("audiobook.chapter", 3);
    CHECK(options.GetInt("audiobook.chapter") == 3);
}

TEST_CASE("Engine.Options.UnreadableFileIsArchivedAndReset", "[engine][options]") {
    TempDirectory temp;
    const auto path = temp.Path() / "settings.json";
    std::ofstream(path) << "not json";
    GameOptions options(path);
    CHECK(options.Load() == OptionsLoadResult::Reset);
    CHECK_FALSE(options.Contains("game.difficulty"));
    GameOptions reloaded(path);
    CHECK(reloaded.Load() == OptionsLoadResult::Loaded);
}

TEST_CASE("Engine.Options.NewSettingsUseFallbackAndPreserveExistingValues", "[engine][options]") {
    TempDirectory temp;
    const auto path = temp.Path() / "settings.json";
    GameOptions options(path);
    (void)options.Load();
    options.SetInt("game.language", 2);
    options.Save();

    GameOptions restored(path);
    REQUIRE(restored.Load() == OptionsLoadResult::Loaded);
    CHECK(restored.GetInt("game.language") == 2);
    CHECK(restored.GetInt("game.new_setting", 3) == 3);
    CHECK_FALSE(restored.Contains("game.new_setting"));
    restored.SetInt("game.new_setting", 4);
    restored.Save();
    REQUIRE(options.Load() == OptionsLoadResult::Loaded);
    CHECK(options.GetInt("game.language") == 2);
    CHECK(options.GetInt("game.new_setting") == 4);
}

TEST_CASE("Engine.Options.OutdatedFormatStartsFreshWithoutResetWarning", "[engine][options]") {
    TempDirectory temp;
    const auto path = temp.Path() / "settings.json";
    std::ofstream(path) << R"({"version":1,"values":{"game.language":2}})";
    std::ofstream(path.string() + ".bak") << R"({"version":1,"values":{"game.language":2}})";
    GameOptions options(path);
    CHECK(options.Load() == OptionsLoadResult::Missing);
    CHECK_FALSE(options.Contains("game.language"));
    CHECK_FALSE(std::filesystem::exists(path));
    CHECK_FALSE(std::filesystem::exists(path.string() + ".bak"));
}
