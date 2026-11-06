#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Session/AssetLibrary.h"
#include "Editor/Internals/Session/AssetNames.h"
#include "Editor/Internals/Session/AssetNavigation.h"

// The asset library's browser: back and search on top, folder tree on the left, a tile grid on the right. Graphics
// open in the shared viewport; a sound loads into the Audio Preview panel and plays there.
class AssetsPanel {
public:
    void SetRoots(const std::filesystem::path& engineDirectory, const std::filesystem::path& assetsDirectory);
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions);

private:
    enum class TypeFilter { All, Graphic, Sound, Tuning, Code, Font, Other };
    // Asset catalog entries by registration file, in catalog order.
    using GraphicScenes = std::map<std::string, std::vector<const SceneDefinition*>>;
    // The New folder / New graphic form. It stays open until the command answers: a refusal shows under the name, and
    // only a success closes it and reveals the new item.
    struct NameDialog {
        AssetNameKind kind = AssetNameKind::Folder;
        std::string folder; // Where it goes, under the project's graphics, as `python Engine asset` takes it
        std::filesystem::path folderPath; // The same folder on disk
        std::array<char, 64> name{};
        bool open = false;
        bool focus = false;
        std::uint64_t request = 0; // The command it waits for; 0 while editing
        std::string submitted;     // The name it sent
        std::string error;         // The command's refusal of `submitted`
    };
    // A just-created item: highlighted, and scrolled to once it appears in the grid.
    struct CreatedItem {
        std::filesystem::path folder;
        std::string name;
        bool scroll = false;
    };
    void Toolbar(const EditorSnapshot& snapshot, const EditorUiActions& actions, const AssetNode& folder);
    void Breadcrumbs(const EditorSnapshot& snapshot, const std::vector<const AssetNode*>& trail);
    void Filters();
    void ResetFilters();
    [[nodiscard]] bool MatchesType(const AssetNode& node) const;
    // The node's path as `python Engine asset` takes it; nullopt outside the project's graphics.
    [[nodiscard]] std::optional<std::string> GraphicsRelative(const AssetNode& node) const;
    void CreateItems(const AssetNode& folder);
    void OpenNameDialog(AssetNameKind kind, const AssetNode& folder);
    void DragSource(const AssetNode& node);
    void DropTarget(const AssetNode& folder, const EditorUiActions& actions) const;
    void DrawNameDialog(const EditorSnapshot& snapshot, const EditorUiActions& actions);
    [[nodiscard]] std::vector<std::string> TakenNames(const NameDialog& dialog) const;
    [[nodiscard]] bool IsCreated(const AssetNode& node) const;
    void TreeNode(const AssetNode& node, int depth, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                  const GraphicScenes& scenes);
    void Grid(const EditorSnapshot& snapshot, const EditorUiActions& actions, const AssetNode& folder,
              const GraphicScenes& scenes);
    void Footer(bool readOnly);
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
    std::optional<CreatedItem> _created;
    std::set<std::string> _expanded; // Keys of open tree rows
    bool _revealFolder = true;       // Open the tree down to the shown folder
    std::array<char, 256> _search{};
    bool _focusSearch = false;
    TypeFilter _typeFilter = TypeFilter::All;
    float _tileSize = 92;
    int _tileIndex = 0;
    int _tileColumns = 1;
};
