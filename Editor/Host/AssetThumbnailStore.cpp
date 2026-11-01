#include "Editor/Host/AssetThumbnailStore.h"
#include <fstream>
#include <sstream>
#include <system_error>
#include "Editor/Internals/Session/GraphicAssetCatalog.h"

namespace {
constexpr int LoadsPerFrame = 8;
constexpr const char* StampFile = "worker.stamp";

std::string WorkerStamp(const std::filesystem::path& executable) {
    std::error_code error;
    const auto written = std::filesystem::last_write_time(executable, error);
    if (error) return {};
    return std::to_string(written.time_since_epoch().count());
}
std::string ReadStamp(const std::filesystem::path& file) {
    std::ifstream input(file);
    std::stringstream text;
    text << input.rdbuf();
    return text.str();
}
} // namespace

AssetThumbnailStore::~AssetThumbnailStore() { Unload(); }

void AssetThumbnailStore::Unload() {
    for (const auto& [id, texture] : _textures)
        if (texture.id) UnloadTexture(texture);
    _textures.clear();
}

void AssetThumbnailStore::Configure(const ProcessCommand& worker, const std::filesystem::path& directory,
                                    const std::string& background) {
    _worker = worker;
    _directory = directory;
    _background = background;
}

void AssetThumbnailStore::Finish(const std::function<void(const std::string&)>& logProblem) {
    const auto code = _process.ExitCode();
    if (code && *code == 0) {
        std::ofstream(_directory / StampFile) << _runningStamp;
        Unload();
    } else {
        _failedStamp = _runningStamp;
        std::string detail;
        for (const auto& line : _process.DrainOutput())
            detail += "\n" + line;
        logProblem("Could not generate asset thumbnails" + detail);
    }
    _process.Clear();
    _running = false;
}

void AssetThumbnailStore::Tick(bool building, bool hasAssets,
                               const std::function<void(const std::string&)>& logProblem) {
    _loadBudget = LoadsPerFrame;
    if (!_worker) return;
    if (_running) {
        if (building) {
            _process.Terminate();
            _process.Clear();
            _running = false;
        } else if (!_process.Running()) {
            Finish(logProblem);
        }
        return;
    }
    if (building || !hasAssets) return;
    const auto stamp = WorkerStamp(_worker->executable);
    if (stamp.empty() || stamp == _failedStamp || stamp == ReadStamp(_directory / StampFile)) return;
    auto command = *_worker;
    command.arguments = {"--thumbnails", _directory.string(), _background};
    try {
        _process.Start(command);
        _running = true;
        _runningStamp = stamp;
    } catch (const std::exception& error) {
        _failedStamp = stamp;
        logProblem(std::string("Could not start asset thumbnails: ") + error.what());
    }
}

std::uint64_t AssetThumbnailStore::Texture(const std::string& id) {
    const auto found = _textures.find(id);
    if (found != _textures.end()) return found->second.id;
    if (_loadBudget <= 0) return 0;
    --_loadBudget;
    const auto file = _directory / AssetThumbnailFile(id);
    Texture2D texture{};
    if (std::filesystem::exists(file)) {
        texture = LoadTexture(file.string().c_str());
        if (texture.id) {
            GenTextureMipmaps(&texture); // Tiles show them far below their pixel size
            SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
        }
    }
    _textures.emplace(id, texture);
    return texture.id;
}
