#pragma once
#include <filesystem>
#include <string>
#include <vector>

enum class AssetKind { Folder, Graphic, Audio, Font, Code, File };

// One row of the asset library. Files sharing a name up to the first dot ("Goblin.h", "Goblin.asset.cpp",
// "Goblin.tuning.json") are one node; `path` is its registration file when there is one.
struct AssetNode {
    std::string name;
    std::filesystem::path path;
    AssetKind kind = AssetKind::File;
    bool readOnly = false;
    std::vector<std::string> files;
    std::vector<AssetNode> children; // Folders first, then by name
};

// Scans the disk, so the tree survives a broken build. Roots: "Engine" (its Assets/Graphics, read-only) and "Game"
// (the project's asset folder). An empty path leaves that root out; missing folders scan as empty.
[[nodiscard]] std::vector<AssetNode> ScanAssetLibrary(const std::filesystem::path& engineDirectory,
                                                      const std::filesystem::path& assetsDirectory);
// By extension, as the library lists sounds.
[[nodiscard]] bool IsAudioFile(const std::filesystem::path& file);
