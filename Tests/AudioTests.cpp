#include <catch2/catch_approx.hpp>
#include "Tests/Support/Diagnostics.h"
#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include "Audio/AudioSystem.hpp"
#include "Audio/Internals/AudioVolumePolicy.h"
#include "Options/GameOptions.h"
#include "Tests/Support/TempDirectory.h"

TEST_CASE("Engine.Audio.VolumeStepsAndClamps", "[engine][audio]") {
    AudioVolumePolicy policy;
    float volume = 0.0f;
    for (int percent : {25, 50, 75, 100}) {
        volume = policy.Step(volume, 1);
        CHECK(AudioVolumePolicy::Percent(volume) == percent);
    }
    CHECK(policy.Step(volume, 1) == 1.0f);
    CHECK(policy.Step(0.0f, -1) == 0.0f);
    CHECK(policy.Step(0.5f, std::numeric_limits<int>::max()) == 1.0f);
    CHECK(policy.Step(0.5f, std::numeric_limits<int>::min()) == 0.0f);
    CHECK(policy.Step(0.62f, -1) == Catch::Approx(0.25f));
    CHECK(policy.Step(0.63f, 0) == Catch::Approx(0.75f));
    CHECK(policy.Step(1.0f, -4) == 0.0f);
}

TEST_CASE("Engine.Audio.MissingAndInvalidSettingsDefaultIndependently", "[engine][audio][options]") {
    TempDirectory temp;
    GameOptions options(temp.Path() / "settings.json");
    AudioVolumePolicy policy;
    CHECK(policy.LoadEffects(options) == 1.0f);
    CHECK(policy.LoadMusic(options) == 1.0f);
    CHECK(policy.LoadMenuMusic(options) == 1.0f);
    options.Set(EngineOption::MusicVolume, 25);
    options.Set(EngineOption::MenuMusicVolume, 75);
    for (auto invalid :
         {std::int64_t{-1}, std::int64_t{101}, std::int64_t{30}, std::numeric_limits<std::int64_t>::max()}) {
        options.Set(EngineOption::EffectsVolume, invalid);
        CHECK(policy.LoadEffects(options) == 1.0f);
        CHECK(policy.LoadMusic(options) == Catch::Approx(0.25f));
        CHECK(policy.LoadMenuMusic(options) == Catch::Approx(0.75f));
    }
    options.Set(EngineOption::EffectsVolume, 0);
    options.Set(EngineOption::MusicVolume, -25);
    CHECK(policy.LoadEffects(options) == 0.0f);
    CHECK(policy.LoadMusic(options) == 1.0f);
    options.Set(EngineOption::MenuMusicVolume, 30);
    CHECK(policy.LoadMenuMusic(options) == 1.0f);
    CHECK(policy.LoadEffects(options) == 0.0f);
}

TEST_CASE("Engine.Audio.VolumeSettingsRoundTripWithoutAutomaticWrites", "[engine][audio][options]") {
    TempDirectory temp;
    const auto path = temp.Path() / "settings.json";
    GameOptions options(path);
    AudioVolumePolicy policy;
    options.SetInt("progress.bestScore", 123);
    for (int percent : {0, 25, 50, 75, 100}) {
        policy.Store(options, static_cast<float>(percent) / 100.0f, static_cast<float>(100 - percent) / 100.0f, 0.5f);
        CHECK(options.Get(EngineOption::EffectsVolume, -1) == percent);
        CHECK(policy.LoadMusic(options) == Catch::Approx(static_cast<float>(100 - percent) / 100.0f));
        CHECK(options.Get(EngineOption::MenuMusicVolume, -1) == 50);
        if (percent == 0) CHECK_FALSE(std::filesystem::exists(path));
        options.Save();
        GameOptions restored(path);
        (void)restored.Load();
        CHECK(policy.LoadEffects(restored) == Catch::Approx(static_cast<float>(percent) / 100.0f));
        CHECK(policy.LoadMusic(restored) == Catch::Approx(static_cast<float>(100 - percent) / 100.0f));
        CHECK(policy.LoadMenuMusic(restored) == Catch::Approx(0.5f));
        CHECK(restored.GetInt("progress.bestScore") == 123);
    }
}

TEST_CASE("Engine.Audio.UntouchedDefaultVolumesAreNotWritten", "[engine][audio][options]") {
    TempDirectory temp;
    GameOptions options(temp.Path() / "settings.json");
    AudioVolumePolicy policy;
    policy.Store(options, 1.0f, 1.0f, 1.0f);
    CHECK_FALSE(options.Dirty());
    CHECK_FALSE(options.Contains(EngineOption::EffectsVolume));
    policy.Store(options, 0.5f, 1.0f, 1.0f);
    CHECK(options.Get(EngineOption::EffectsVolume, -1) == 50);
    CHECK_FALSE(options.Contains(EngineOption::MusicVolume));
    policy.Store(options, 1.0f, 1.0f, 1.0f);
    CHECK(options.Get(EngineOption::EffectsVolume, -1) == 100);
}

TEST_CASE("Engine.Audio.ConfigurableSteps", "[engine][audio][options]") {
    TempDirectory temp;
    GameOptions options(temp.Path() / "settings.json");
    AudioVolumePolicy policy({.steps = 5});
    CHECK(policy.Step(0.0f, 1) == Catch::Approx(0.2f));
    policy.Store(options, 0.4f, 0.8f, 0.2f);
    CHECK(options.Get(EngineOption::EffectsVolume, -1) == 40);
    CHECK(options.Get(EngineOption::MusicVolume, -1) == 80);
    CHECK(options.Get(EngineOption::MenuMusicVolume, -1) == 20);
    CHECK(policy.LoadEffects(options) == Catch::Approx(0.4f));
    CHECK(policy.LoadMusic(options) == Catch::Approx(0.8f));
    CHECK(policy.LoadMenuMusic(options) == Catch::Approx(0.2f));
    CHECK_THROWS_AS(AudioVolumePolicy(AudioVolumeSettings{.steps = 0}), std::invalid_argument);
    CHECK_THROWS_AS(AudioVolumePolicy(AudioVolumeSettings{.steps = 3}), std::invalid_argument);
}

TEST_CASE("Engine.Audio.CatalogValidationIsHeadlessAndIdsAreIndependent", "[engine][audio]") {
    CHECK_NOTHROW(AudioSystem::ValidateCatalog({}));
    const auto lastId = std::numeric_limits<std::size_t>::max();
    AudioCatalog catalog{.effects = {{SoundId{lastId},
                                      "Audio/not-installed.wav",
                                      {.maxConcurrent = 1, .volume = 0.5f, .overflow = EffectOverflow::Skip}}},
                         .music = {{MusicId{lastId}, "Audio/not-installed.ogg", MusicVolumeGroup::Menu}}};
    CHECK_NOTHROW(AudioSystem::ValidateCatalog(catalog));
}

TEST_CASE("Engine.Audio.CatalogRejectsDuplicateIdsWithinEachResourceKind", "[engine][audio]") {
    AudioCatalog catalog{.effects = {{SoundId{0}, "first.wav"}, {SoundId{0}, "second.wav"}}, .music = {}};
    CHECK_THROWS_AS(AudioSystem::ValidateCatalog(catalog), std::invalid_argument);
    catalog = {.effects = {}, .music = {{MusicId{0}, "first.ogg"}, {MusicId{0}, "second.ogg"}}};
    CHECK_THROWS_AS(AudioSystem::ValidateCatalog(catalog), std::invalid_argument);
}

TEST_CASE("Engine.Audio.CatalogRejectsInvalidPathsAndSettings", "[engine][audio]") {
    AudioCatalog catalog{.effects = {{SoundId{0}, "click.wav"}}, .music = {{MusicId{0}, "menu.ogg"}}};
    const auto invalidPaths = {std::filesystem::path{}, std::filesystem::absolute("outside.wav"),
                               std::filesystem::path{"../outside.wav"}, std::filesystem::path{"Audio/../outside.wav"}};
    SECTION("Effect paths") {
        for (const auto& path : invalidPaths) {
            catalog.effects.front().file = path;
            CHECK_THROWS_AS(AudioSystem::ValidateCatalog(catalog), std::invalid_argument);
        }
    }
    SECTION("Music paths") {
        for (const auto& path : invalidPaths) {
            catalog.music.front().file = path;
            CHECK_THROWS_AS(AudioSystem::ValidateCatalog(catalog), std::invalid_argument);
        }
    }
    SECTION("No voices") {
        catalog.effects.front().settings.maxConcurrent = 0;
        CHECK_THROWS_AS(AudioSystem::ValidateCatalog(catalog), std::invalid_argument);
    }
    SECTION("Invalid gain") {
        for (float volume :
             {-0.1f, 1.1f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
            catalog.effects.front().settings.volume = volume;
            CHECK_THROWS_AS(AudioSystem::ValidateCatalog(catalog), std::invalid_argument);
        }
    }
    SECTION("Invalid overflow policy") {
        catalog.effects.front().settings.overflow = static_cast<EffectOverflow>(-1);
        CHECK_THROWS_AS(AudioSystem::ValidateCatalog(catalog), std::invalid_argument);
    }
    SECTION("Invalid music group") {
        catalog.music.front().volumeGroup = static_cast<MusicVolumeGroup>(-1);
        CHECK_THROWS_AS(AudioSystem::ValidateCatalog(catalog), std::invalid_argument);
    }
}
