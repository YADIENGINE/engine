#include "Editor/Host/ProjectListLauncher.h"
#include <windows.h>
#include <system_error>
#include <vector>

namespace {
constexpr const wchar_t* LauncherVariable = L"ENGINE_LAUNCHER";
} // namespace
std::filesystem::path LauncherFromEnvironment() {
    std::size_t length = 0;
    if (_wgetenv_s(&length, nullptr, 0, LauncherVariable) != 0 || length <= 1) return {};
    std::wstring value(length, L'\0');
    if (_wgetenv_s(&length, value.data(), value.size(), LauncherVariable) != 0) return {};
    value.resize(length - 1);
    std::filesystem::path launcher(value);
    std::error_code error;
    return launcher.is_absolute() && std::filesystem::is_regular_file(launcher, error) ? launcher
                                                                                       : std::filesystem::path{};
}
std::string StartLauncher(const std::filesystem::path& launcher) {
    const std::wstring quoted = L"\"" + launcher.wstring() + L"\"";
    std::vector<wchar_t> commandLine(quoted.begin(), quoted.end());
    commandLine.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const std::wstring directory = launcher.parent_path().wstring();
    if (!CreateProcessW(launcher.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, directory.c_str(),
                        &startup, &process))
        return "Could not start the launcher: " + std::system_category().message(static_cast<int>(GetLastError()));
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return {};
}
