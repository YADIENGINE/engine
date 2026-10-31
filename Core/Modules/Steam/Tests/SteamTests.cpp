#include "Tests/Support/Diagnostics.h"
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include "Modules/Steam/Steam.h"

namespace {
std::optional<std::string> GetVariable(const char* name) {
#ifdef _WIN32
    char* value = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || !value) return std::nullopt;
    std::string text(value);
    std::free(value);
    return text;
#else
    const char* value = std::getenv(name);
    return value ? std::optional<std::string>(value) : std::nullopt;
#endif
}

void SetVariable(const char* name, const std::optional<std::string>& value) {
#ifdef _WIN32
    // An empty value removes the variable on Windows.
    _putenv_s(name, value ? value->c_str() : "");
#else
    if (value)
        setenv(name, value->c_str(), 1);
    else
        unsetenv(name);
#endif
}

// Sets a launch variable for one test and restores what the runner had.
class ScopedVariable {
public:
    ScopedVariable(const char* name, const std::optional<std::string>& value)
        : _name(name)
        , _previous(GetVariable(name)) {
        SetVariable(_name, value);
    }
    ~ScopedVariable() { SetVariable(_name, _previous); }

private:
    const char* _name;
    std::optional<std::string> _previous;

public:
    ScopedVariable(const ScopedVariable&) = delete;
    ScopedVariable& operator=(const ScopedVariable&) = delete;
};
} // namespace

static_assert(SteamAccount{1}.SteamId64() == SteamAccount{0}.SteamId64() + 1);
static_assert(Steam::IsValidAchievementName("ACH_WIN_ONE_GAME"));
static_assert(Steam::IsValidAchievementName("ach_7"));
static_assert(!Steam::IsValidAchievementName(""));
static_assert(!Steam::IsValidAchievementName("ACH WIN"));
static_assert(!Steam::IsValidAchievementName("ACH-WIN"));
static_assert(!Steam::IsValidAchievementName("ACH_\xC5\x9E")); // UTF-8 letter

TEST_CASE("Steam.Achievements.NameLengthLimit", "[steam]") {
    CHECK(Steam::IsValidAchievementName(std::string(128, 'A')));
    CHECK_FALSE(Steam::IsValidAchievementName(std::string(129, 'A')));
}

TEST_CASE("Steam.Achievements.DisconnectedCallsAreSafe", "[steam]") {
    Steam steam;
    // No Steamworks backend is compiled, so no machine state may connect achievements.
    CHECK_FALSE(steam.AchievementsAvailable());
    CHECK_NOTHROW(steam.UnlockAchievement("ACH_WIN_ONE_GAME"));
    CHECK_NOTHROW(steam.UnlockAchievement("ACH_WIN_ONE_GAME"));
    CHECK_NOTHROW(steam.Update());
    CHECK_THROWS_AS(steam.UnlockAchievement(""), std::invalid_argument);
    CHECK_THROWS_AS(steam.UnlockAchievement("ACH WIN"), std::invalid_argument);
}

TEST_CASE("Steam.Launch.AppIdMarksSteamLaunch", "[steam]") {
    const ScopedVariable gameId("SteamGameId", std::nullopt);
    const ScopedVariable appId("SteamAppId", "480");
    const Steam steam;
    CHECK(steam.Status().launchedBySteam);
    REQUIRE(steam.Status().appId);
    CHECK(*steam.Status().appId == 480u);
    CHECK(steam.Status().source == SteamSource::Environment);
}

TEST_CASE("Steam.Launch.GameIdFallbackKeepsAppBits", "[steam]") {
    const ScopedVariable appId("SteamAppId", std::nullopt);
    // A mod/shortcut game id carries extra bits above the 24-bit app id.
    const ScopedVariable gameId("SteamGameId", std::to_string((1ull << 32) | 480u));
    const Steam steam;
    CHECK(steam.Status().launchedBySteam);
    REQUIRE(steam.Status().appId);
    CHECK(*steam.Status().appId == 480u);
}

TEST_CASE("Steam.Launch.InvalidOrMissingIdsAreNotSteamLaunch", "[steam]") {
    const ScopedVariable gameId("SteamGameId", std::nullopt);
    for (const char* text : {"", "0", "abc", "480x", "-5"}) {
        const ScopedVariable appId("SteamAppId", text[0] ? std::optional<std::string>(text) : std::nullopt);
        const Steam steam;
        INFO("SteamAppId=" << text);
        CHECK_FALSE(steam.Status().launchedBySteam);
        CHECK_FALSE(steam.Status().appId);
    }
}

TEST_CASE("Steam.Status.StaysConsistentOnAnyMachine", "[steam]") {
    // The runner may or may not have a Steam client; only relations between fields are fixed.
    const ScopedVariable appId("SteamAppId", std::nullopt);
    const ScopedVariable gameId("SteamGameId", std::nullopt);
    const Steam steam;
    const auto& status = steam.Status();
    CHECK_FALSE(status.launchedBySteam);
    CHECK(status.source != SteamSource::Sdk);
    if (status.account) {
        CHECK(status.clientRunning);
        CHECK(status.account->accountId != 0u);
    }
    CHECK((status.source == SteamSource::None) == !status.clientRunning);
}
