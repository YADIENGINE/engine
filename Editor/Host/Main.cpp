#include <windows.h>
#include <shellapi.h>
#include <array>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include "Editor/Host/EditorEntryPoint.h"
#include "Editor/Internals/Platform/Windows/WindowsText.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    try {
        std::array<wchar_t, 32768> executable{};
        const DWORD size = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (!size || size == executable.size()) throw std::runtime_error("Cannot resolve editor executable path");
        std::filesystem::path project =
            std::filesystem::path(executable.data()).parent_path() / "EngineEditor.project.json";
        int argc = 0;
        const auto release = [](wchar_t** pointer) {
            if (pointer) LocalFree(pointer);
        };
        std::unique_ptr<wchar_t*, decltype(release)> arguments(CommandLineToArgvW(GetCommandLineW(), &argc), release);
        if (!arguments) ThrowWorkerWindowsError("Read editor arguments");
        if (argc == 3 && std::wstring(arguments.get()[1]) == L"--project")
            project = arguments.get()[2];
        else if (argc != 1)
            throw std::runtime_error("Usage: EngineEditor [--project file.json]");
        return RunEngineEditor(project);
    } catch (const std::exception& error) {
        MessageBoxW(nullptr, WorkerWide(error.what()).c_str(), L"Engine Editor", MB_OK | MB_ICONERROR);
        return 1;
    }
}
