#pragma once
#include <array>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Session/AssetLibrary.h"
#include "Editor/Internals/Session/AssetNavigation.h"

// The asset library's browser: back and search on top, folder tree on the left, a tile grid on the right. Graphics
// open in the shared viewport; sounds play from a bar below the grid, leaving the running scene alone.
class AssetsPanel {
public:
    void SetRoots(const std::filesystem::path& engineDirectory, const std::filesystem::path& assetsDirectory);
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions);

private:
    // Asset catalog entries by registration file, in catalog order.
    using GraphicScenes = std::map<std::string, std::vector<const SceneDefinition*>>;
    void Toolbar(const EditorSnapshot& snapshot, const EditorUiActions& actions);
    void TreeNode(const AssetNode& node, int depth, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                  const GraphicScenes& scenes);
    void Grid(const EditorSnapshot& snapshot, const EditorUiActions& actions, const AssetNode& folder,
              const GraphicScenes& scenes);
    void Footer();
    void AudioBar(const EditorSnapshot& snapshot, const EditorUiActions& actions);
    void NodeTile(const AssetNode& node, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                  const GraphicScenes& scenes);
    void SceneTile(const SceneDefinition& scene, const std::string& label, const AssetNode& node,
                   const EditorSnapshot& snapshot, const EditorUiActions& actions);
    void Open(const SceneDefinition& scene, const EditorSnapshot& snapshot, const EditorUiActions& actions);
    void GoBack(const EditorSnapshot& snapshot, const EditorUiActions& actions);
    [[nodiscard]] bool CanGoBack(const EditorSnapshot& snapshot) const;
    void Enter(const AssetLocation& location, const EditorSnapshot& snapshot);
    void PlayAudio(const AssetNode& node, const EditorUiActions& actions);
    void NextTile();

    std::filesystem::path _engineDirectory;
    std::filesystem::path _assetsDirectory;
    std::vector<AssetNode> _roots;
    double _nextScan = 0;
    AssetNavigation _navigation;
    std::set<std::string> _expanded; // Keys of open tree rows
    bool _revealFolder = true;       // Open the tree down to the shown folder
    std::array<char, 256> _search{};
    bool _focusSearch = false;
    float _tileSize = 92;
    std::filesystem::path _audioFile; // The sound in the bar
    std::string _audioName;
    bool _audioLoop = false;
    float _audioVolume = 1.0f;
    int _tileIndex = 0;
    int _tileColumns = 1;
};
