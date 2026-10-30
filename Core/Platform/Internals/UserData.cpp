#include "Platform/Internals/UserData.h"
#include <cstdlib>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#ifdef _WIN32
std::filesystem::path UserDataDirectory() {
    for (const wchar_t* name : {L"LOCALAPPDATA", L"APPDATA"}) {
        const DWORD length = GetEnvironmentVariableW(name, nullptr, 0);
        if (length == 0) continue;
        std::wstring value(length, L'\0');
        const DWORD written = GetEnvironmentVariableW(name, value.data(), length);
        if (written > 0 && written < length) {
            value.resize(written);
            return std::filesystem::path(value);
        }
    }
    throw std::runtime_error("LOCALAPPDATA/APPDATA is unavailable; pass a save path explicitly");
}
#else
std::filesystem::path UserDataDirectory() {
    const char* home = std::getenv("HOME");
#ifdef __APPLE__
    if (home && *home) return std::filesystem::path(home) / "Library" / "Application Support";
#else
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg && std::filesystem::path(xdg).is_absolute()) return xdg;
    if (home && *home) return std::filesystem::path(home) / ".local" / "share";
#endif
    throw std::runtime_error("User data directory is unavailable; pass a save path explicitly");
}
#endif
