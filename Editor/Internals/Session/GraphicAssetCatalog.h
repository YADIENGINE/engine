#pragma once
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "Editor/Api/GraphicAsset.h"
#include "Editor/Api/SceneRegistry.h"

// Comparable form of a path: the worker's source paths and the editor's disk scan meet here.
[[nodiscard]] std::string AssetPathKey(const std::filesystem::path& path);
// One catalog entry per item, hidden from the scene catalog; ids are `asset:Graphics/Folder/Name[#Item]`, with
// `Engine/` after the colon for the engine's own assets. Throws on a file outside `Assets/Graphics` or a bad item.
[[nodiscard]] std::vector<SceneDefinition> GraphicAssetDefinitions(const GraphicAsset& asset,
                                                                   const std::filesystem::path& file,
                                                                   const std::filesystem::path& engineRoot);
void AddRegisteredGraphicAssets(SceneRegistry& registry);

struct GraphicThumbnail {
    std::string id; // Its catalog entry
    std::shared_ptr<const GraphicAsset> asset;
    std::size_t item = 0;
};
[[nodiscard]] std::vector<GraphicThumbnail> RegisteredGraphicThumbnails();
// The thumbnail's file name for a catalog id; ids hold characters file names cannot.
[[nodiscard]] std::string AssetThumbnailFile(const std::string& id);
