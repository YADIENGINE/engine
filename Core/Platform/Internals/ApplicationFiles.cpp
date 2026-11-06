#include "Platform/Internals/ApplicationFiles.h"
#include <raylib.h>
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include "Platform/Internals/UserData.h"
#include "Runtime/Internals/StartupTimer.h"

namespace {
// `Assets` beside the executable. ENGINE_ASSET_DIRECTORY replaces it: the editor runs its worker from a snapshot copy
// of the executable (WorkerSnapshots), whose folder holds no assets, and points it at the build's own `Assets`.
std::filesystem::path AssetDirectory() {
#ifdef _WIN32
    std::size_t length = 0;
    if (_wgetenv_s(&length, nullptr, 0, L"ENGINE_ASSET_DIRECTORY") == 0 && length > 1) {
        std::wstring value(length, L'\0');
        if (_wgetenv_s(&length, value.data(), value.size(), L"ENGINE_ASSET_DIRECTORY") == 0) {
            value.resize(length - 1); // Without the terminating null
            return std::filesystem::path(value);
        }
    }
#else
    if (const char* value = std::getenv("ENGINE_ASSET_DIRECTORY"); value && *value) return std::filesystem::path(value);
#endif
    return std::filesystem::path(GetApplicationDirectory()) / "Assets";
}
} // namespace

std::filesystem::path PrepareAssets(std::span<const std::filesystem::path> files) {
    const StartupTimer timer("Prepare assets");
    for (const auto& file : files) {
        if (file.empty() || file.has_root_path()) throw std::invalid_argument("Asset path must be relative");
        for (const auto& part : file) {
            if (part == "..") throw std::invalid_argument("Asset path cannot leave its root");
        }
    }
    const auto directory = AssetDirectory();
    for (const auto& file : files) {
        if (!std::filesystem::is_regular_file(directory / file)) {
            throw std::runtime_error("Asset not found: " + (directory / file).string());
        }
    }
    return directory;
}

bool AssetReportAsked() {
#ifdef _WIN32
    std::size_t length = 0;
    getenv_s(&length, nullptr, 0, "ENGINE_ASSET_REPORT");
    return length > 0;
#else
    const char* value = std::getenv("ENGINE_ASSET_REPORT");
    return value && *value;
#endif
}

std::filesystem::path SaveFilePath(const std::filesystem::path& applicationId, const std::filesystem::path& filename) {
    return UserDataDirectory() / applicationId / filename;
}
