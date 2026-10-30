#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

// None: no Steam trace; Environment: read from launch environment/registry, unverified; Sdk: confirmed by Steam API
enum class SteamSource { None, Environment, Sdk };

struct SteamAccount {
    // 32-bit account id; the folder name Steam itself uses under its userdata directory
    std::uint32_t accountId = 0;
    // 64-bit SteamID of an individual account in the public universe
    [[nodiscard]] constexpr std::uint64_t SteamId64() const { return IndividualBase + accountId; }
    constexpr bool operator==(const SteamAccount&) const = default;

private:
    static constexpr std::uint64_t IndividualBase = 76561197960265728ull;
};

struct SteamStatus {
    bool clientRunning = false;          // a live Steam client process is registered for this OS user
    bool launchedBySteam = false;        // Steam started this process (launch environment carries an app id)
    std::optional<std::uint32_t> appId;  // app id from the launch environment
    std::optional<SteamAccount> account; // account logged in to the running client
    SteamSource source = SteamSource::None;
};

// Steam detection and achievement forwarding; never fails when Steam is absent.
class Steam final {
public:
    // Probes once; the status does not change for the lifetime of this object.
    Steam();
    [[nodiscard]] const SteamStatus& Status() const { return _status; }

    // True only when the Steam API is connected (SteamSource::Sdk); unlocks are dropped otherwise.
    [[nodiscard]] bool AchievementsAvailable() const { return _status.source == SteamSource::Sdk; }
    // Idempotent. Throws std::invalid_argument for an invalid API name even when Steam is absent.
    void UnlockAchievement(std::string_view apiName);
    // Call once per frame; sends pending unlocks when connected.
    void Update();
    // API names Steamworks accepts: 1..128 ASCII letters, digits or underscores.
    [[nodiscard]] static constexpr bool IsValidAchievementName(std::string_view apiName) {
        if (apiName.empty() || apiName.size() > MaxAchievementName) return false;
        for (const char c : apiName) {
            const bool letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
            if (!letter && !(c >= '0' && c <= '9') && c != '_') return false;
        }
        return true;
    }

private:
    static constexpr std::size_t MaxAchievementName = 128;
    SteamStatus _status;

public:
    Steam(const Steam&) = delete;
    Steam& operator=(const Steam&) = delete;
};
