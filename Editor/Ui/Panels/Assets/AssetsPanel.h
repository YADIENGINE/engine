#pragma once
#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Session/AssetLibrary.h"
#include "Editor/Internals/Session/AssetNavigation.h"

// The asset library's browser: back and search on top, folder tree on the left, a tile grid on the right. Graphics
// open in the shared viewport; a sound loads into the Audio Preview panel and plays there.
class AssetsPanel {
public:
    void SetRoots(const std::filesystem::path& engineDirectory, const std::filesystem::path& assetsDirectory);
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions);

private:
    // Asset catalog entries by registration file, in catalog order.
    using GraphicScenes = std::map<std::string, std::vector<const SceneDefinition*>>;
    struct NameDialog {
        enum Kind { Folder, Graphic };
        Kind kind = Folder;
        std::string folder; // Where it goes, under the project's graphics
        std::array<char, 64> name{};
        bool open = false;
        bool focus = false;
    };
    void Toolbar(const EditorSnapshot& snapshot, const EditorUiActions& actions, const AssetNode& folder);
    // The node's path as `python Engine asset` takes it; nullopt outside the project's graphics.
    [[nodiscard]] std::optional<std::string> GraphicsRelative(const AssetNode& node) const;
    void CreateItems(const AssetNode& folder);
    void DragSource(const AssetNode& node);
    void DropTarget(const AssetNode& folder, const EditorUiActions& actions) const;
    void DrawNameDialog(const EditorUiActions& actions);
    void TreeNode(const AssetNode& node, int depth, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                  const GraphicScenes& scenes);
    void Grid(const EditorSnapshot& snapshot, const EditorUiActions& actions, const AssetNode& folder,
              const GraphicScenes& scenes);
    void Footer();
    void NodeTile(const AssetNode& node, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                  const GraphicScenes& scenes);
    void SceneTile(const SceneDefinition& scene, const std::string& label, const AssetNode& node,
                   const EditorSnapshot& snapshot, const EditorUiActions& actions);
    void Open(const SceneDefinition& scene, const EditorSnapshot& snapshot, const EditorUiActions& actions);
    void GoBack(const EditorSnapshot& snapshot, const EditorUiActions& actions);
    [[nodiscard]] bool CanGoBack(const EditorSnapshot& snapshot) const;
    void Enter(const AssetLocation& location, const EditorSnapshot& snapshot);
    void PlayAudio(const AssetNode& node, const EditorSnapshot& snapshot, const EditorUiActions& actions);
    void NextTile();

    std::filesystem::path _engineDirectory;
    std::filesystem::path _assetsDirectory;
    std::vector<AssetNode> _roots;
    double _nextScan = 0;
    AssetNavigation _navigation;
    NameDialog _dialog;
    std::set<std::string> _expanded; // Keys of open tree rows
    bool _revealFolder = true;       // Open the tree down to the shown folder
    std::array<char, 256> _search{};
    bool _focusSearch = false;
    float _tileSize = 92;
    int _tileIndex = 0;
    int _tileColumns = 1;
};
