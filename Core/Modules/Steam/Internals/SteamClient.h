#pragma once
#include <cstdint>
#include <optional>

struct SteamClientProbe {
    bool running = false;
    std::optional<std::uint32_t> accountId; // only when running and logged in
};

// Windows: HKCU\Software\Valve\Steam\ActiveProcess (pid, ActiveUser). Other platforms report nothing.
[[nodiscard]] SteamClientProbe ReadSteamClient();
