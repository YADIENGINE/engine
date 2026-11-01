#include "Editor/Ui/Panels/Assets/AssetsPanel.h"
#include <extras/IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <format>
#include "Editor/Internals/Session/GraphicAssetCatalog.h"
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr double ScanSeconds = 2.0;
constexpr float TreeWidth = 220;
constexpr float TreeRowHeight = 28;
constexpr float TreeIndent = 16;
constexpr float TreeArrowWidth = 18;
constexpr float SizeSliderWidth = 110;
constexpr float MinTileSize = 64;
constexpr float MaxTileSize = 160;
constexpr float TileWheelStep = 8;
constexpr float TileIconShare = 0.42f; // Of the tile's width
constexpr float VolumeSliderWidth = 120;
constexpr int BackMouseButton = 3;

const char* KindIcon(AssetKind kind) {
    switch (kind) {
    case AssetKind::Folder : return ICON_FA_FOLDER;
    case AssetKind::Graphic: return ICON_FA_IMAGE;
    case AssetKind::Audio  : return ICON_FA_MUSIC;
    case AssetKind::Font   : return ICON_FA_FONT;
    case AssetKind::Code   : return ICON_FA_CODE;
    case AssetKind::File   : return ICON_FA_FILE;
    }
    return ICON_FA_FILE;
}

ImVec4 KindColor(AssetKind kind) {
    switch (kind) {
    case AssetKind::Folder : return ToolToneColor(ToolTone::Warning);
    case AssetKind::Graphic: return ToolToneColor(ToolTone::Accent);
    case AssetKind::Audio  : return ToolToneColor(ToolTone::Success);
    case AssetKind::Font   : return ToolToneColor(ToolTone::Information);
    case AssetKind::Code   :
    case AssetKind::File   : return ToolPalette::Secondary;
    }
    return ToolPalette::Secondary;
}

struct TileLook {
    const char* id;
    const char* label;
    const char* icon;
    ImVec4 color;
    bool selected = false;
    bool dimmed = false;
    int count = 0; // Shown as a badge above one
};

bool DrawTile(const TileLook& look, float size) {
    const float scale = ToolUiScale();
    const float side = size * scale;
    const float labelHeight = ImGui::GetTextLineHeightWithSpacing();
    const float radius = ToolMetrics::Radius * scale;
    const float pad = ToolMetrics::Space * scale;
    ImGui::PushID(look.id);
    const bool pressed = ImGui::InvisibleButton("##tile", {side, side + labelHeight});
    const bool hovered = ImGui::IsItemHovered();
    const auto start = ImGui::GetItemRectMin();
    const auto end = ImGui::GetItemRectMax();
    auto* draw = ImGui::GetWindowDrawList();
    if (look.selected || hovered)
        draw->AddRectFilled(start, end, ImGui::GetColorU32(look.selected ? ToolPalette::Selected : ToolPalette::Hover),
                            radius);
    if (look.selected) draw->AddRect(start, end, ImGui::GetColorU32(ToolPalette::Accent), radius, 0, scale);
    const auto iconColor = look.dimmed ? ToolPalette::Disabled : look.color;
    const float iconSize = side * TileIconShare;
    const auto iconExtent = ImGui::GetFont()->CalcTextSizeA(iconSize, FLT_MAX, 0.0f, look.icon);
    draw->AddText(ImGui::GetFont(), iconSize,
                  {start.x + (side - iconExtent.x) * 0.5f, start.y + (side - iconExtent.y) * 0.5f},
                  ImGui::GetColorU32(iconColor), look.icon);
    if (look.count > 1) {
        const auto text = std::to_string(look.count);
        const auto extent = ImGui::CalcTextSize(text.c_str());
        const ImVec2 badgeEnd{end.x - pad, start.y + pad + extent.y + pad};
        draw->AddRectFilled({badgeEnd.x - extent.x - pad * 2, start.y + pad}, badgeEnd,
                            ImGui::GetColorU32(ToolToneColor(ToolTone::Accent)), radius);
        draw->AddText({badgeEnd.x - extent.x - pad, start.y + pad * 1.5f}, ImGui::GetColorU32(ToolPalette::Window),
                      text.c_str());
    }
    const float labelY = start.y + side;
    const float labelStart = start.x + pad;
    const float labelEnd = end.x - pad;
    const float labelWidth = ImGui::CalcTextSize(look.label).x;
    // Centered when it fits; a long name starts at the tile's edge and ends in an ellipsis.
    const float labelX = std::max(labelStart, start.x + (side - labelWidth) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, look.dimmed ? ToolPalette::Disabled : ToolPalette::Text);
    ImGui::RenderTextEllipsis(draw, {labelX, labelY}, {labelEnd, labelY + ImGui::GetTextLineHeight()}, labelEnd,
                              look.label, nullptr, nullptr);
    ImGui::PopStyleColor();
    ImGui::PopID();
    return pressed;
}

struct TreeRow {
    const char* id;
    const char* label;
    const char* icon;
    ImVec4 iconColor;
    int depth = 0;
    bool expandable = false;
    bool expanded = false;
    bool selected = false;
    bool root = false;
};
enum class TreeClick { None, Row, Arrow };

// The whole row is one target, and rows touch: there is no gap to miss. Guides show which folder a row belongs to.
TreeClick DrawTreeRow(const TreeRow& row) {
    const float scale = ToolUiScale();
    const float height = TreeRowHeight * scale;
    const float indent = TreeIndent * scale;
    const float arrowWidth = TreeArrowWidth * scale;
    const float gap = ToolMetrics::Gap * scale;
    const float rowWidth = ImGui::GetContentRegionAvail().x;
    const auto click = ImGui::InvisibleButton(row.id, {rowWidth, height});
    const bool hovered = ImGui::IsItemHovered();
    const auto start = ImGui::GetItemRectMin();
    const auto end = ImGui::GetItemRectMax();
    auto* draw = ImGui::GetWindowDrawList();
    const float radius = ToolMetrics::Radius * scale;
    if (row.root) draw->AddRectFilled(start, end, ImGui::GetColorU32(ToolPalette::Control), radius);
    if (row.selected || hovered)
        draw->AddRectFilled(start, end, ImGui::GetColorU32(row.selected ? ToolPalette::Selected : ToolPalette::Hover),
                            radius);
    if (row.selected)
        draw->AddRectFilled({start.x, start.y + scale * 3}, {start.x + scale * 3, end.y - scale * 3},
                            ImGui::GetColorU32(ToolPalette::Accent), scale);
    for (int level = 0; level < row.depth; ++level) {
        const float x = start.x + indent * static_cast<float>(level) + arrowWidth * 0.5f;
        draw->AddLine({x, start.y}, {x, end.y}, ImGui::GetColorU32(ToolPalette::Separator), scale);
    }
    const float arrowX = start.x + indent * static_cast<float>(row.depth);
    const float textY = start.y + (height - ImGui::GetTextLineHeight()) * 0.5f;
    if (row.expandable) {
        const char* arrow = row.expanded ? ICON_FA_CHEVRON_DOWN : ICON_FA_CHEVRON_RIGHT;
        const auto extent = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize() * 0.75f, FLT_MAX, 0.0f, arrow);
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.75f,
                      {arrowX + (arrowWidth - extent.x) * 0.5f, start.y + (height - extent.y) * 0.5f},
                      ImGui::GetColorU32(ToolPalette::Secondary), arrow);
    }
    const float iconX = arrowX + arrowWidth;
    draw->AddText({iconX, textY}, ImGui::GetColorU32(row.iconColor), row.icon);
    const float labelX = iconX + ImGui::GetFontSize() + gap;
    ImGui::PushStyleColor(ImGuiCol_Text, row.root ? ToolPalette::Text : ToolPalette::Secondary);
    if (row.selected) ImGui::PushStyleColor(ImGuiCol_Text, ToolPalette::Text);
    ImGui::RenderTextEllipsis(draw, {labelX, textY}, {end.x - gap, textY + ImGui::GetTextLineHeight()}, end.x - gap,
                              row.label, nullptr, nullptr);
    if (row.selected) ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    if (!click) return TreeClick::None;
    const bool onArrow = row.expandable && ImGui::GetMousePos().x < arrowX + arrowWidth;
    return onArrow ? TreeClick::Arrow : TreeClick::Row;
}

const AssetNode* FindFolder(const std::vector<AssetNode>& nodes, const std::string& key,
                            std::vector<const AssetNode*>& trail) {
    for (const auto& node : nodes) {
        if (node.kind != AssetKind::Folder) continue;
        trail.push_back(&node);
        if (AssetPathKey(node.path) == key) return &node;
        if (const auto* found = FindFolder(node.children, key, trail)) return found;
        trail.pop_back();
    }
    return nullptr;
}

const AssetNode* FindNode(const std::vector<AssetNode>& nodes, const std::string& key) {
    for (const auto& node : nodes) {
        if (AssetPathKey(node.path) == key) return &node;
        if (const auto* found = FindNode(node.children, key)) return found;
    }
    return nullptr;
}

void CollectFiles(const std::vector<AssetNode>& nodes, std::vector<const AssetNode*>& result) {
    for (const auto& node : nodes) {
        if (node.kind != AssetKind::Folder) result.push_back(&node);
        CollectFiles(node.children, result);
    }
}

void AssetMenu(const AssetNode& node, const std::string& usage, const EditorUiActions& actions) {
    if (!ImGui::BeginPopupContextItem("asset-menu")) return;
    if (!usage.empty() && ImGui::MenuItem(ICON_FA_COPY "  Copy usage")) ImGui::SetClipboardText(usage.c_str());
    if (ImGui::MenuItem(ICON_FA_COPY "  Copy path")) ImGui::SetClipboardText(node.path.generic_string().c_str());
    if ((node.kind == AssetKind::Graphic || node.kind == AssetKind::Code) &&
        ImGui::MenuItem(ICON_FA_CODE "  Open code") && actions.openPath)
        actions.openPath(node.path);
    if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  Show in Explorer") && actions.openPath)
        actions.openPath(node.kind == AssetKind::Folder ? node.path : node.path.parent_path());
    ImGui::EndPopup();
}

std::string OpenScene(const EditorSnapshot& snapshot) {
    return snapshot.selected ? snapshot.selected->id : std::string{};
}
} // namespace

void AssetsPanel::SetRoots(const std::filesystem::path& engineDirectory, const std::filesystem::path& assetsDirectory) {
    _engineDirectory = engineDirectory;
    _assetsDirectory = assetsDirectory;
    _nextScan = 0;
}

void AssetsPanel::Enter(const AssetLocation& location, const EditorSnapshot& snapshot) {
    _navigation.Go(location, OpenScene(snapshot));
    _revealFolder = true;
}

void AssetsPanel::Open(const SceneDefinition& scene, const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    _navigation.SceneOpened(OpenScene(snapshot), scene.id);
    actions.select(scene.id);
    actions.command(EditorCommand::Open);
}

bool AssetsPanel::CanGoBack(const EditorSnapshot& snapshot) const {
    const auto* step = _navigation.Peek();
    // Going back in the panel alone is always possible; changing the scene waits for a busy editor.
    return step && (!snapshot.busy || step->scene == OpenScene(snapshot));
}

void AssetsPanel::GoBack(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    const auto step = _navigation.Back();
    if (!step) return;
    _revealFolder = true;
    if (step->scene == OpenScene(snapshot)) return;
    if (step->scene.empty()) {
        actions.command(EditorCommand::Stop);
    } else if (std::ranges::find(snapshot.catalog, step->scene, &SceneDefinition::id) != snapshot.catalog.end()) {
        actions.select(step->scene);
        actions.command(EditorCommand::Open);
    }
}

void AssetsPanel::Toolbar(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    const bool canGoBack = CanGoBack(snapshot);
    ImGui::BeginDisabled(!canGoBack);
    if (ToolIconButton("back", ICON_FA_ARROW_LEFT, "Back (Alt+Left)")) GoBack(snapshot, actions);
    ImGui::EndDisabled();
    const bool panelFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    const bool backKey = panelFocused && ImGui::GetIO().KeyAlt && ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false);
    const bool backMouse =
        ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && ImGui::IsMouseClicked(BackMouseButton);
    if (canGoBack && (backKey || backMouse)) GoBack(snapshot, actions);
    if (panelFocused && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F)) _focusSearch = true;
    ImGui::SameLine();
    const bool hasText = _search[0] != '\0';
    const float clear = hasText ? ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x : 0.0f;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - clear);
    if (_focusSearch) {
        ImGui::SetKeyboardFocusHere();
        _focusSearch = false;
    }
    ImGui::InputTextWithHint("##asset-search", ICON_FA_MAGNIFYING_GLASS "  Search assets (Ctrl+F)", _search.data(),
                             _search.size());
    if (hasText) {
        ImGui::SameLine();
        if (ToolIconButton("clear-search", ICON_FA_XMARK, "Clear search")) _search.fill('\0');
    }
}

void AssetsPanel::TreeNode(const AssetNode& node, int depth, const EditorSnapshot& snapshot,
                           const EditorUiActions& actions, const GraphicScenes& scenes) {
    const auto key = AssetPathKey(node.path);
    const auto scene = scenes.find(key);
    const bool isSet = node.kind == AssetKind::Graphic && scene != scenes.end() && scene->second.size() > 1;
    if (node.kind != AssetKind::Folder && !isSet) return;
    const auto& location = _navigation.Location();
    const bool expandable = std::ranges::any_of(node.children, [&](const AssetNode& child) {
        return child.kind == AssetKind::Folder ||
               (child.kind == AssetKind::Graphic && scenes.contains(AssetPathKey(child.path)) &&
                scenes.at(AssetPathKey(child.path)).size() > 1);
    });
    const bool expanded = expandable && _expanded.contains(key);
    const bool selected = isSet ? location.set == key : location.set.empty() && location.folder == key;
    const bool root = depth == 0;
    const char* icon = isSet                   ? ICON_FA_LAYER_GROUP
                       : root && node.readOnly ? ICON_FA_LOCK
                       : expanded              ? ICON_FA_FOLDER_OPEN
                                               : ICON_FA_FOLDER;
    const auto iconColor = isSet  ? KindColor(AssetKind::Graphic)
                           : root ? ToolPalette::Secondary
                                  : KindColor(AssetKind::Folder);
    ImGui::PushID(key.c_str());
    const auto click =
        DrawTreeRow({"##row", node.name.c_str(), icon, iconColor, depth, expandable, expanded, selected, root});
    if (root && node.readOnly) ToolTooltip("Read-only: assets that come with the engine");
    AssetMenu(node, {}, actions);
    ImGui::PopID();
    if (click == TreeClick::Arrow) {
        if (expanded)
            _expanded.erase(key);
        else
            _expanded.insert(key);
    } else if (click == TreeClick::Row) {
        // A second click on the shown row folds it; a first click opens it, so browsing rarely needs the arrow.
        if (selected && expanded)
            _expanded.erase(key);
        else
            _expanded.insert(key);
        _search.fill('\0');
        if (isSet)
            Enter({AssetPathKey(node.path.parent_path()), key}, snapshot);
        else
            Enter({key, {}}, snapshot);
    }
    if (expanded)
        for (const auto& child : node.children)
            TreeNode(child, depth + 1, snapshot, actions, scenes);
}

void AssetsPanel::NextTile() {
    if (++_tileIndex % _tileColumns != 0) ImGui::SameLine();
}

void AssetsPanel::SceneTile(const SceneDefinition& scene, const std::string& label, const AssetNode& node,
                            const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    const bool selected = snapshot.selected && snapshot.selected->id == scene.id;
    const bool enabled = !snapshot.busy || (selected && snapshot.capabilities.Can(EditorCommand::Open));
    ImGui::BeginDisabled(!enabled);
    const bool pressed = DrawTile(
        {scene.id.c_str(), label.c_str(), KindIcon(AssetKind::Graphic), KindColor(AssetKind::Graphic), selected},
        _tileSize);
    ImGui::EndDisabled();
    if (pressed) Open(scene, snapshot, actions);
    ToolTooltip(scene.description.c_str());
    ImGui::PushID(scene.id.c_str());
    AssetMenu(node, scene.asset->usage, actions);
    ImGui::PopID();
    NextTile();
}

void AssetsPanel::PlayAudio(const AssetNode& node, const EditorUiActions& actions) {
    _audioFile = node.path;
    _audioName = node.name;
    if (actions.previewAudio)
        actions.previewAudio(
            {AudioPreviewAction::Play, node.path.lexically_relative(_assetsDirectory), _audioLoop, _audioVolume});
}

void AssetsPanel::NodeTile(const AssetNode& node, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                           const GraphicScenes& scenes) {
    const auto key = AssetPathKey(node.path);
    const auto found = node.kind == AssetKind::Graphic ? scenes.find(key) : scenes.end();
    if (found != scenes.end() && found->second.size() == 1) {
        SceneTile(*found->second.front(), node.name, node, snapshot, actions);
        return;
    }
    const int count = found != scenes.end() ? static_cast<int>(found->second.size()) : 0;
    const bool notBuilt = node.kind == AssetKind::Graphic && found == scenes.end();
    const bool sound = node.kind == AssetKind::Audio;
    const bool playing = sound && node.path == _audioFile && snapshot.audioPlaying;
    const char* icon = count > 1 ? ICON_FA_LAYER_GROUP : playing ? ICON_FA_VOLUME_HIGH : KindIcon(node.kind);
    const bool pressed = DrawTile({key.c_str(), node.name.c_str(), icon, KindColor(node.kind), node.path == _audioFile,
                                   notBuilt || node.kind == AssetKind::Code, count},
                                  _tileSize);
    if (notBuilt) {
        ToolTooltip("Not built yet: use Build & Refresh");
    } else if (!node.files.empty()) {
        std::string files;
        for (const auto& file : node.files)
            files += (files.empty() ? "" : "\n") + file;
        ToolTooltip(files.c_str());
    }
    if (pressed) {
        if (node.kind == AssetKind::Folder) {
            Enter({key, {}}, snapshot);
        } else if (count > 1) {
            _search.fill('\0');
            Enter({AssetPathKey(node.path.parent_path()), key}, snapshot);
        } else if (sound) {
            if (playing && actions.previewAudio)
                actions.previewAudio({AudioPreviewAction::Stop, {}, _audioLoop, _audioVolume});
            else
                PlayAudio(node, actions);
        }
    }
    ImGui::PushID(key.c_str());
    AssetMenu(node, {}, actions);
    ImGui::PopID();
    NextTile();
}

void AssetsPanel::Grid(const EditorSnapshot& snapshot, const EditorUiActions& actions, const AssetNode& folder,
                       const GraphicScenes& scenes) {
    const auto spacing = ImGui::GetStyle().ItemSpacing.x;
    _tileColumns = std::max(
        1, static_cast<int>((ImGui::GetContentRegionAvail().x + spacing) / (_tileSize * ToolUiScale() + spacing)));
    _tileIndex = 0;
    const auto query = EditorSearchText(_search.data());
    if (!query.empty()) {
        std::vector<const AssetNode*> nodes;
        CollectFiles(_roots, nodes);
        for (const auto* node : nodes) {
            const auto found = scenes.find(AssetPathKey(node->path));
            const bool isSet = found != scenes.end() && found->second.size() > 1;
            if (EditorSearchText(node->name).find(query) != std::string::npos) {
                NodeTile(*node, snapshot, actions, scenes);
            } else if (isSet) {
                for (const auto* scene : found->second)
                    if (EditorSearchText(scene->name + " " + scene->description).find(query) != std::string::npos)
                        SceneTile(*scene, scene->asset->item, *node, snapshot, actions);
            }
        }
        if (_tileIndex == 0) ToolEmptyState("No matching assets", "Try another name or tag.");
        return;
    }
    if (const auto& set = _navigation.Location().set; !set.empty()) {
        const auto found = scenes.find(set);
        const auto* node = FindNode(_roots, set);
        if (found != scenes.end() && node) {
            for (const auto* scene : found->second)
                SceneTile(*scene, scene->asset->item, *node, snapshot, actions);
            return;
        }
        // The set is gone after a rebuild: show its folder.
        _navigation.Correct({_navigation.Location().folder, {}});
    }
    for (const auto& node : folder.children)
        NodeTile(node, snapshot, actions, scenes);
    if (folder.children.empty())
        ToolEmptyState("Empty folder", folder.readOnly ? "The engine has no assets here yet."
                                                       : "Add an X.asset.cpp file here to list a graphic.");
}

void AssetsPanel::Footer() {
    const float scale = ToolUiScale();
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", std::format("{} {}", _tileIndex, _tileIndex == 1 ? "item" : "items").c_str());
    ImGui::SameLine(ImGui::GetContentRegionMax().x - SizeSliderWidth * scale);
    ImGui::SetNextItemWidth(SizeSliderWidth * scale);
    ImGui::SliderFloat("##tile-size", &_tileSize, MinTileSize, MaxTileSize, "");
    ToolTooltip("Tile size (Ctrl+wheel over the grid)");
}

void AssetsPanel::AudioBar(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    const float scale = ToolUiScale();
    const bool playing = snapshot.audioPlaying;
    if (ToolIconButton("audio-play", playing ? ICON_FA_STOP : ICON_FA_PLAY, playing ? "Stop" : "Play") &&
        actions.previewAudio) {
        if (playing)
            actions.previewAudio({AudioPreviewAction::Stop, {}, _audioLoop, _audioVolume});
        else
            actions.previewAudio(
                {AudioPreviewAction::Play, _audioFile.lexically_relative(_assetsDirectory), _audioLoop, _audioVolume});
    }
    ImGui::SameLine();
    if (ToolIconButton("audio-loop", ICON_FA_REPEAT, "Loop on the next play", _audioLoop)) _audioLoop = !_audioLoop;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(VolumeSliderWidth * scale);
    if (ImGui::SliderFloat("##audio-volume", &_audioVolume, 0.0f, 1.0f, "%.2f") && actions.previewAudio)
        actions.previewAudio({AudioPreviewAction::Volume, {}, _audioLoop, _audioVolume});
    ToolTooltip("Volume");
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(_audioName.c_str());
}

void AssetsPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    if (ImGui::GetTime() >= _nextScan) {
        _roots = ScanAssetLibrary(_engineDirectory, _assetsDirectory);
        _nextScan = ImGui::GetTime() + ScanSeconds;
    }
    if (_roots.empty()) {
        ToolEmptyState("No asset folders", "Pass ASSETS to engine_add_editor_worker to list the project's assets.");
        return;
    }
    std::vector<const AssetNode*> trail;
    const AssetNode* folder = FindFolder(_roots, _navigation.Location().folder, trail);
    if (!folder) { // First draw, or the folder was removed: start at the project's graphics
        const std::filesystem::path start = !_assetsDirectory.empty() ? _assetsDirectory / "Graphics" : _roots[0].path;
        _navigation.Correct({AssetPathKey(start), {}});
        _revealFolder = true;
        trail.clear();
        folder = FindFolder(_roots, _navigation.Location().folder, trail);
        if (!folder) {
            trail.assign(1, &_roots[0]);
            folder = &_roots[0];
            _navigation.Correct({AssetPathKey(folder->path), {}});
        }
    }
    if (_revealFolder) {
        for (const auto* step : trail)
            _expanded.insert(AssetPathKey(step->path));
        for (const auto& root : _roots)
            _expanded.insert(AssetPathKey(root.path));
        _revealFolder = false;
    }
    GraphicScenes scenes;
    for (const auto& entry : snapshot.catalog)
        if (entry.asset) scenes[entry.asset->file].push_back(&entry);

    const float scale = ToolUiScale();
    const float padding = ToolMetrics::Gap;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ToolMetrics::Gap * scale, ToolMetrics::Gap * scale});
    if (BeginToolPanel("asset-toolbar", {0, ImGui::GetFrameHeight() + padding * 2 * scale}, ToolSurface::Toolbar, 0,
                       ImGuiWindowFlags_NoScrollbar, padding))
        Toolbar(snapshot, actions);
    EndToolPanel();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ToolMetrics::Gap * scale); // The panes touch the toolbar
    if (BeginToolPanel("asset-tree", {TreeWidth * scale, 0}, ToolSurface::Utility, ImGuiChildFlags_ResizeX, 0,
                       ToolMetrics::Space)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
        for (const auto& root : _roots)
            TreeNode(root, 0, snapshot, actions, scenes);
        ImGui::PopStyleVar();
    }
    EndToolPanel();
    ImGui::SameLine(0, 0);
    if (BeginToolPanel("asset-content", {0, 0}, ToolSurface::Content, 0, ImGuiWindowFlags_NoScrollbar, padding)) {
        const float rows = (_audioFile.empty() ? 1.0f : 2.0f);
        const float reserve = ImGui::GetFrameHeightWithSpacing() * rows;
        const bool resizing = ImGui::GetIO().KeyCtrl;
        if (ImGui::BeginChild("asset-grid", {0, -reserve}, 0, resizing ? ImGuiWindowFlags_NoScrollWithMouse : 0)) {
            Grid(snapshot, actions, *folder, scenes);
            if (resizing && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::GetIO().MouseWheel != 0.0f)
                _tileSize = std::clamp(_tileSize + ImGui::GetIO().MouseWheel * TileWheelStep, MinTileSize, MaxTileSize);
        }
        ImGui::EndChild();
        if (!_audioFile.empty()) AudioBar(snapshot, actions);
        Footer();
    }
    EndToolPanel();
    ImGui::PopStyleVar();
}
