#include "Editor/Internals/Platform/Windows/CommandLocator.h"
#include <windows.h>
#include <array>
#include <filesystem>
#include "Editor/Internals/Platform/Windows/WindowsText.h"

bool CommandOnPath(const std::string& program) {
    if (program.empty()) return false;
    const auto wide = WorkerWide(program);
    std::error_code error;
    if (wide.find_first_of(L"\\/") != std::wstring::npos) return std::filesystem::is_regular_file(wide, error);
    std::array<wchar_t, MAX_PATH> found{};
    for (const wchar_t* extension : {L".exe", L".cmd", L".bat", L".ps1"})
        if (SearchPathW(nullptr, wide.c_str(), extension, static_cast<DWORD>(found.size()), found.data(), nullptr) > 0)
            return true;
    return false;
}
