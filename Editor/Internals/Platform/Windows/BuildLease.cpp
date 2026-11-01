#include "Editor/Internals/Platform/Windows/BuildLease.h"
#include <windows.h>
#include <cstdint>
#include <cwctype>
#include <stdexcept>
#include <string>
#include "Editor/Internals/Platform/Windows/WindowsText.h"

BuildLease::BuildLease(const std::filesystem::path& directory) {
    const auto path = std::filesystem::weakly_canonical(directory).wstring();
    std::uint64_t hash = 14695981039346656037ull;
    for (wchar_t character : path) {
        hash ^= static_cast<std::uint64_t>(std::towlower(character));
        hash *= 1099511628211ull;
    }
    const auto name = L"Local\\EngineEditor.Build." + std::to_wstring(hash);
    _mutex.Reset(CreateMutexW(nullptr, FALSE, name.c_str()));
    if (!_mutex) ThrowWorkerWindowsError("Create build lock");
    const DWORD wait = WaitForSingleObject(_mutex.Get(), 0);
    _owned = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
    if (!_owned) throw std::runtime_error("Another editor is already building in this directory");
}
BuildLease::~BuildLease() {
    if (_owned) ReleaseMutex(_mutex.Get());
}
