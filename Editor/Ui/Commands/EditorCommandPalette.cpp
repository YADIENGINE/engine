#include "Editor/Ui/Commands/EditorCommandPalette.h"
#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "Editor/Internals/Session/AssetLibrary.h"
#include "Editor/Internals/Session/QuickOpen.h"
#include "Editor/Ui/Commands/EditorActions.h"
#include "Editor/Ui/Commands/EditorCommands.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr const char* PaletteId = "Quick open###editor.commands";
constexpr std::size_t MaxShown = 80;
constexpr float KindColumn = 72.0f;
constexpr int RecentBonus = 40; // Between a word start and a mere match: among equals, what was opened lately wins
struct PaletteEntry {
    const char* kind;
    std::string label;
    std::string detail;
    const char* shortcut;
    bool enabled;
    std::string reason;
    std::function<void()> execute;
    int score = 0;
};
void CollectSounds(const std::vector<AssetNode>& nodes, const std::string& folder, auto& add) {
    for (const auto& node : nodes) {
        if (node.kind == AssetKind::Audio) add(node, folder);
        CollectSounds(node.children, folder.empty() ? node.name : folder + "/" + node.name, add);
    }
}
// A row: its kind, name and where it lives, with any shortcut at the far end.
bool DrawRow(const PaletteEntry& entry, bool selected) {
    const float scale = ToolUiScale();
    const auto start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const bool pressed = ImGui::Selectable("##row", selected, ImGuiSelectableFlags_DontClosePopups);
    auto* draw = ImGui::GetWindowDrawList();
    const float inset = ToolMetrics::Space * scale;
    const float labelX = start.x + KindColumn * scale;
    float right = start.x + width - inset;
    if (entry.shortcut && *entry.shortcut) {
        ImGui::PushFont(ToolFont(ToolFontRole::Technical), ToolMetrics::CaptionSize);
        const float shortcutWidth = ImGui::CalcTextSize(entry.shortcut).x;
        draw->AddText({right - shortcutWidth, start.y}, ImGui::GetColorU32(ToolPalette::Muted), entry.shortcut);
        right -= shortcutWidth + inset * 2;
        ImGui::PopFont();
    }
    draw->AddText({start.x + inset, start.y}, ImGui::GetColorU32(ToolPalette::Muted), entry.kind);
    const auto label = ToolClipText(entry.label.c_str(), std::max(1.0f, right - labelX));
    draw->AddText({labelX, start.y}, ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
    if (!entry.detail.empty()) {
        const float detailX = labelX + ImGui::CalcTextSize(label.c_str()).x + inset * 2;
        if (const auto detail = ToolClipText(entry.detail.c_str(), right - detailX); detailX < right && !detail.empty())
            draw->AddText({detailX, start.y}, ImGui::GetColorU32(ToolPalette::Muted), detail.c_str());
    }
    return pressed;
}
} // namespace

void EditorCommandPalette::SetAssetRoots(const std::filesystem::path& engineDirectory,
                                         const std::filesystem::path& assetsDirectory) {
    _engineDirectory = engineDirectory;
    _assetsDirectory = assetsDirectory;
}
void EditorCommandPalette::ReadSounds() {
    _sounds.clear();
    auto add = [this](const AssetNode& node, const std::string& folder) {
        _sounds.push_back({node.name, folder, std::filesystem::absolute(node.path)});
    };
    CollectSounds(ScanAssetLibrary(_engineDirectory, _assetsDirectory), {}, add);
}
bool EditorCommandPalette::Draw(const EditorActionContext& context) {
    const auto& snapshot = context.snapshot;
    const auto& actions = context.actions;
    auto& preferences = context.preferences;
    auto& result = context.result;
    const bool opening = result.openCommands || result.openQuick;
    if (opening) {
        _commandsOnly = result.openCommands;
        _search.fill('\0');
        _selected = 0;
        if (!_commandsOnly) ReadSounds();
        ImGui::OpenPopup(PaletteId);
    }
    const auto* viewport = ImGui::GetMainViewport();
    const float scale = ToolUiScale();
    const float width = std::max(1.0f, std::min(640.0f * scale, viewport->Size.x - 32.0f * scale));
    // Over the toolbar's search box, as if it had opened; centered until the toolbar has drawn once.
    const float margin = width * 0.5f + 16.0f * scale;
    const float centerX = _anchorTop > 0 ? _anchorCenterX : viewport->Pos.x + viewport->Size.x * 0.5f;
    ImGui::SetNextWindowPos(
        {std::clamp(centerX, viewport->Pos.x + margin,
                    std::max(viewport->Pos.x + margin, viewport->Pos.x + viewport->Size.x - margin)),
         _anchorTop > 0 ? _anchorTop : viewport->Pos.y + ImGui::GetFrameHeight() * 3.0f},
        ImGuiCond_Appearing, {0.5f, 0});
    ImGui::SetNextWindowSize({width, 0}, ImGuiCond_Appearing);
    if (!ImGui::BeginPopup(PaletteId,
                           ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        const bool closing = _wasOpen;
        _wasOpen = false;
        return opening || closing;
    }
    _wasOpen = true;
    if (opening || _focusSearch) ImGui::SetKeyboardFocusHere();
    _focusSearch = false;
    ImGui::SetNextItemWidth(-1);
    const bool enter = ImGui::InputTextWithHint(
        "##command-search", _commandsOnly ? "Type a command..." : "Search scenes, graphics, sounds and commands...",
        _search.data(), _search.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    const bool searchChanged = ImGui::IsItemEdited();
    if (searchChanged) _selected = 0;
    const std::string_view query(_search.data());

    std::vector<PaletteEntry> entries;
    const auto runScene = [&](const SceneDefinition& scene, const SceneParameter* parameter,
                              const std::string* choice) {
        actions.select(scene.id);
        if (parameter && choice) actions.parameter(parameter->id, *choice);
        actions.command(EditorCommand::Open);
    };
    if (!_commandsOnly) {
        const char* busyReason = "Wait for the current operation to finish.";
        const std::string reason = snapshot.busy ? busyReason : "";
        for (const auto& scene : snapshot.catalog) {
            entries.push_back({scene.asset ? "Graphic" : "Scene", scene.name,
                               scene.asset ? scene.asset->set : scene.category, "", !snapshot.busy, reason,
                               [&, scenePointer = &scene] { runScene(*scenePointer, nullptr, nullptr); }});
            // Only a search reaches inside a scene: its choices would drown the list otherwise.
            if (query.empty() || scene.asset) continue;
            for (const auto& parameter : scene.parameters) {
                if (parameter.kind != SceneParameterKind::Choice) continue;
                for (const auto& choice : parameter.choices)
                    entries.push_back(
                        {"Scene", scene.name, parameter.label + ": " + choice, "", !snapshot.busy, reason,
                         [&, scenePointer = &scene, parameterPointer = &parameter, choicePointer = &choice] {
                             runScene(*scenePointer, parameterPointer, choicePointer);
                         }});
            }
        }
        for (const auto& sound : _sounds)
            entries.push_back({"Sound", sound.name, sound.folder, "", true, {}, [&, path = sound.path] {
                                   preferences.Visible(EditorPanel::AudioPreview) = true;
                                   actions.previewAudio(
                                       {AudioPreviewAction::Play, path, snapshot.audio.loop, snapshot.audio.volume});
                               }});
    }
    const std::size_t commandsStart = entries.size();
    // The menus' actions (EditorActionList): each under its menu's name, with its submenu and state beside it.
    const auto list = EditorActionList(context);
    for (const auto& action : list) {
        if (!action.palette || !action.run) continue;
        std::string detail = action.submenu;
        const char* state = action.check == EditorActionCheck::Toggle  ? (action.checked ? "On" : "Off")
                            : action.check == EditorActionCheck::Radio ? (action.checked ? "Current" : "")
                                                                       : "";
        if (*state) detail += detail.empty() ? state : std::string(" \xC2\xB7 ") + state;
        entries.push_back({EditorMenuTitle(action.menu), action.label, std::move(detail), action.shortcut,
                           action.enabled, action.reason, action.run});
    }

    // Empty search: the scenes opened last, then the commands. A search: the best fit, whatever its kind.
    std::map<std::string, int> recentScenes;
    for (std::size_t index = 0; index < preferences.recent.size(); ++index)
        if (const auto scene = std::ranges::find(snapshot.catalog, preferences.recent[index], &SceneDefinition::id);
            scene != snapshot.catalog.end())
            recentScenes.try_emplace(scene->name, static_cast<int>(index));
    if (query.empty() && !_commandsOnly) {
        std::vector<PaletteEntry> ordered;
        for (std::size_t index = 0; index < commandsStart; ++index) {
            const auto recent = recentScenes.find(entries[index].label);
            if (recent == recentScenes.end()) continue;
            entries[index].score = -recent->second;
            ordered.push_back(std::move(entries[index]));
        }
        std::ranges::stable_sort(ordered, std::greater{}, &PaletteEntry::score);
        for (std::size_t index = commandsStart; index < entries.size(); ++index)
            ordered.push_back(std::move(entries[index]));
        entries = std::move(ordered);
    } else if (!query.empty()) {
        for (auto& entry : entries) {
            entry.score = QuickOpenScore(entry.label, entry.detail + " " + entry.kind, query);
            if (entry.score >= 0 && recentScenes.contains(entry.label)) entry.score += RecentBonus;
        }
        std::erase_if(entries, [](const PaletteEntry& entry) { return entry.score < 0; });
        std::ranges::stable_sort(entries, std::greater{}, &PaletteEntry::score);
    }
    if (entries.size() > MaxShown) entries.resize(MaxShown);

    const int count = static_cast<int>(entries.size());
    bool moved = searchChanged;
    if (count > 0) {
        _selected = std::clamp(_selected, 0, count - 1);
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            _selected = (_selected + 1) % count;
            moved = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            _selected = (_selected + count - 1) % count;
            moved = true;
        }
    }
    bool execute = enter && count > 0 && entries[static_cast<std::size_t>(_selected)].enabled;
    if (ImGui::BeginChild("command-results", {0, std::min(320.0f * scale, viewport->Size.y * 0.5f)})) {
        if (entries.empty())
            ToolEmptyState("Nothing found", _commandsOnly ? "Try a command, panel or layout name."
                                                          : "Try a scene, graphic, sound or command name.");
        for (int index = 0; index < count; ++index) {
            auto& entry = entries[static_cast<std::size_t>(index)];
            ImGui::PushID(index);
            ImGui::BeginDisabled(!entry.enabled);
            if (DrawRow(entry, index == _selected)) {
                _selected = index;
                execute = entry.enabled;
            }
            if (index == _selected && moved) ImGui::SetScrollHereY();
            ImGui::EndDisabled();
            if (!entry.enabled) ToolTooltip(entry.reason.c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    if (count > 0 && !entries[static_cast<std::size_t>(_selected)].enabled)
        ImGui::TextDisabled("%s", entries[static_cast<std::size_t>(_selected)].reason.c_str());
    if (execute) {
        entries[static_cast<std::size_t>(_selected)].execute();
        ImGui::CloseCurrentPopup();
    } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
    } else if (enter) {
        // Keep typing possible when Enter targets an unavailable command.
        _focusSearch = true;
    }
    ImGui::EndPopup();
    return true;
}
