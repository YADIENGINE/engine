#pragma once
#include <cstdint>
#include <optional>

// App id Steam puts in the environment of processes it launches (SteamAppId, else SteamGameId).
[[nodiscard]] std::optional<std::uint32_t> ReadLaunchAppId();
