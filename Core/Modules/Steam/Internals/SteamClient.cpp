#include "Modules/Steam/Internals/SteamClient.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#ifdef _WIN32
namespace {
constexpr const wchar_t* ActiveProcessKey = L"Software\\Valve\\Steam\\ActiveProcess";

std::optional<DWORD> ReadDword(const wchar_t* name) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, ActiveProcessKey, name, RRF_RT_REG_DWORD, nullptr, &value, &size) !=
        ERROR_SUCCESS)
        return std::nullopt;
    return value;
}

// The registry keeps the last pid after Steam exits or crashes; only a live process counts.
bool ProcessAlive(DWORD pid) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    DWORD exitCode = 0;
    const bool alive = GetExitCodeProcess(process, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(process);
    return alive;
}
} // namespace

SteamClientProbe ReadSteamClient() {
    SteamClientProbe probe;
    const auto pid = ReadDword(L"pid");
    if (!pid || *pid == 0 || !ProcessAlive(*pid)) return probe;
    probe.running = true;
    // ActiveUser is 0 while the client sits on the login screen.
    if (const auto user = ReadDword(L"ActiveUser"); user && *user != 0) probe.accountId = *user;
    return probe;
}
#else
SteamClientProbe ReadSteamClient() { return {}; }
#endif
