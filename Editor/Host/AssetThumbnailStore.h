#pragma once
#include <raylib.h>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include "Editor/Internals/Platform/Windows/ChildProcess.h"
#include "Editor/Internals/Process/ProcessCommand.h"

// Keeps the Assets panel's thumbnail files fresh and hands them out as textures. A new worker build regenerates them
// in a separate hidden process; until then, including during a broken build, the old files stay in use.
class AssetThumbnailStore {
public:
    AssetThumbnailStore() = default;
    ~AssetThumbnailStore();
    // `background` is the panel color behind the drawings, as RRGGBB.
    void Configure(const ProcessCommand& worker, const std::filesystem::path& directory, const std::string& background);
    // Call once per frame. `building`: a build replaces the worker now, so its file must stay free.
    void Tick(bool building, bool hasAssets, const std::function<void(const std::string&)>& logProblem);
    // The thumbnail's GL texture, 0 while there is none. Loads a limited number of files per frame.
    [[nodiscard]] std::uint64_t Texture(const std::string& id);

private:
    void Finish(const std::function<void(const std::string&)>& logProblem);
    void Unload();
    std::optional<ProcessCommand> _worker;
    std::filesystem::path _directory;
    std::string _background;
    ChildProcess _process;
    bool _running = false;
    std::string _runningStamp;
    std::string _failedStamp; // A build whose generation failed is not retried
    std::map<std::string, Texture2D> _textures;
    int _loadBudget = 0;

public:
    AssetThumbnailStore(const AssetThumbnailStore&) = delete;
    AssetThumbnailStore& operator=(const AssetThumbnailStore&) = delete;
};
