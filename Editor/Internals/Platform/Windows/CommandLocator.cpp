#include "Editor/Internals/Platform/Windows/CommandLocator.h"
#include <windows.h>
#include <array>
#include <filesystem>
#include <string_view>
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

namespace {
std::wstring EnvironmentText(const wchar_t* name) {
    const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (size == 0) return {};
    std::wstring text(size, L'\0');
    text.resize(GetEnvironmentVariableW(name, text.data(), size));
    return text;
}
} // namespace

std::filesystem::path FindGitExecutable() {
    std::wstring found(32768, L'\0');
    if (const auto path = EnvironmentText(L"PATH"); !path.empty()) {
        const DWORD length =
            SearchPathW(path.c_str(), L"git.exe", nullptr, static_cast<DWORD>(found.size()), found.data(), nullptr);
        if (length > 0 && length < found.size()) return found.substr(0, length);
    }
    std::error_code error;
    for (const auto* root : {L"ProgramW6432", L"ProgramFiles", L"LOCALAPPDATA"}) {
        std::filesystem::path base = EnvironmentText(root);
        if (base.empty()) continue;
        if (std::wstring_view(root) == L"LOCALAPPDATA") base /= L"Programs";
        auto candidate = base / L"Git" / L"cmd" / L"git.exe";
        if (std::filesystem::is_regular_file(candidate, error)) return candidate;
    }
    return {};
}
