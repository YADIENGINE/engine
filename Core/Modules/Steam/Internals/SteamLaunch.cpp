#include "Modules/Steam/Internals/SteamLaunch.h"
#include <charconv>
#include <cstdlib>
#include <string>
#include <system_error>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {
// SteamGameId packs the app id into its low 24 bits (higher bits mark mods and shortcuts).
constexpr std::uint64_t GameIdAppMask = 0xFFFFFF;

std::optional<std::string> ReadVariable(const char* name) {
#ifdef _WIN32
    const DWORD length = GetEnvironmentVariableA(name, nullptr, 0);
    if (length == 0) return std::nullopt;
    std::string value(length, '\0');
    const DWORD written = GetEnvironmentVariableA(name, value.data(), length);
    if (written == 0 || written >= length) return std::nullopt;
    value.resize(written);
    return value;
#else
    const char* value = std::getenv(name);
    if (!value) return std::nullopt;
    return std::string(value);
#endif
}

template <typename T> std::optional<T> ParseNumber(const std::string& text) {
    T value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) return std::nullopt;
    return value;
}
} // namespace

std::optional<std::uint32_t> ReadLaunchAppId() {
    if (const auto text = ReadVariable("SteamAppId")) {
        if (const auto appId = ParseNumber<std::uint32_t>(*text); appId && *appId != 0) return appId;
    }
    if (const auto text = ReadVariable("SteamGameId")) {
        if (const auto gameId = ParseNumber<std::uint64_t>(*text)) {
            const auto appId = static_cast<std::uint32_t>(*gameId & GameIdAppMask);
            if (appId != 0) return appId;
        }
    }
    return std::nullopt;
}
