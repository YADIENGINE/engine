#include "Platform/Internals/ApplicationFiles.h"
#include <raylib.h>
#include <array>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include "Platform/Internals/UserData.h"
#include "Runtime/Internals/StartupTimer.h"
#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <android_native_app_glue.h>
extern "C" android_app* GetAndroidApp(void);
#endif

#ifdef __ANDROID__
namespace {
std::filesystem::path AndroidDataDirectory() {
    const android_app* app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->internalDataPath) {
        throw std::runtime_error("Android internal data directory is unavailable");
    }
    return app->activity->internalDataPath;
}

void ExtractAsset(AAssetManager* manager, const std::filesystem::path& directory, const std::filesystem::path& name) {
    std::unique_ptr<AAsset, decltype(&AAsset_close)> asset(
        AAssetManager_open(manager, name.generic_string().c_str(), AASSET_MODE_STREAMING), &AAsset_close);
    if (!asset) throw std::runtime_error(std::string("Android asset is missing: ") + name.string());

    const std::filesystem::path destination = directory / name;
    std::filesystem::create_directories(destination.parent_path());
    std::filesystem::path temporary = destination;
    temporary += ".tmp";
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Could not create Android asset file: " + temporary.string());

        std::array<char, 64 * 1024> buffer{};
        while (true) {
            const int count = AAsset_read(asset.get(), buffer.data(), buffer.size());
            if (count < 0) throw std::runtime_error(std::string("Could not read Android asset: ") + name.string());
            if (count == 0) break;
            output.write(buffer.data(), count);
            if (!output) throw std::runtime_error("Could not write Android asset file: " + temporary.string());
        }
        output.close();
        if (!output) throw std::runtime_error("Could not finish Android asset file: " + temporary.string());
        std::filesystem::rename(temporary, destination);
    } catch (...) {
        std::error_code error;
        std::filesystem::remove(temporary, error);
        throw;
    }
}
} // namespace
#endif

std::filesystem::path PrepareAssets(std::span<const std::filesystem::path> files) {
    const StartupTimer timer("Prepare assets");
    for (const auto& file : files) {
        if (file.empty() || file.has_root_path()) throw std::invalid_argument("Asset path must be relative");
        for (const auto& part : file) {
            if (part == "..") throw std::invalid_argument("Asset path cannot leave its root");
        }
    }
#ifdef __ANDROID__
    const android_app* app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->assetManager) {
        throw std::runtime_error("Android asset manager is unavailable");
    }
    const auto directory = AndroidDataDirectory() / "Assets";
    for (const auto& file : files) {
        ExtractAsset(app->activity->assetManager, directory, file);
    }
    return directory;
#else
    const auto directory = std::filesystem::path(GetApplicationDirectory()) / "Assets";
    for (const auto& file : files) {
        if (!std::filesystem::is_regular_file(directory / file)) {
            throw std::runtime_error("Asset not found: " + (directory / file).string());
        }
    }
    return directory;
#endif
}

std::filesystem::path SaveFilePath(const std::filesystem::path& applicationId, const std::filesystem::path& filename) {
#ifdef __ANDROID__
    return AndroidDataDirectory() / applicationId / filename;
#else
    return UserDataDirectory() / applicationId / filename;
#endif
}
