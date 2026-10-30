#include "Platform/Internals/ApplicationFiles.h"
#include <raylib.h>
#include <stdexcept>
#include "Platform/Internals/UserData.h"
#include "Runtime/Internals/StartupTimer.h"

std::filesystem::path PrepareAssets(std::span<const std::filesystem::path> files) {
    const StartupTimer timer("Prepare assets");
    for (const auto& file : files) {
        if (file.empty() || file.has_root_path()) throw std::invalid_argument("Asset path must be relative");
        for (const auto& part : file) {
            if (part == "..") throw std::invalid_argument("Asset path cannot leave its root");
        }
    }
    const auto directory = std::filesystem::path(GetApplicationDirectory()) / "Assets";
    for (const auto& file : files) {
        if (!std::filesystem::is_regular_file(directory / file)) {
            throw std::runtime_error("Asset not found: " + (directory / file).string());
        }
    }
    return directory;
}

std::filesystem::path SaveFilePath(const std::filesystem::path& applicationId, const std::filesystem::path& filename) {
    return UserDataDirectory() / applicationId / filename;
}
