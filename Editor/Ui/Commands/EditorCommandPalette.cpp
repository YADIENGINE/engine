#include "Editor/Ui/Commands/EditorCommandPalette.h"
#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "Editor/Ui/Commands/EditorCommands.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr const char* PaletteId = "Command palette###editor.commands";
struct PaletteEntry {
    std::string label;
    const char* shortcut;
    bool enabled;
    std::string reason;
    std::function<void()> execute;
};
std::string Lower(std::string_view text) {
    std::string result(text);
    std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}
bool Matches(const std::string& label, const char* search) {
    const auto text = Lower(label);
    const auto query = Lower(search);
    std::size_t start = 0;
    while (start < query.size()) {
        const auto end = query.find(' ', start);
        if (text.find(query.substr(start, end == std::string::npos ? end : end - start)) == std::string::npos)
            return false;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return true;
}
} // namespace
bool EditorCommandPalette::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                EditorPreferences& preferences, bool maximized, EditorShellResult& result) {
    const bool opening = result.openCommands;
    if (opening) {
        _search.fill('\0');
        _selected = 0;
        ImGui::OpenPopup(PaletteId);
    }
    const auto* viewport = ImGui::GetMainViewport();
    const float scale = ToolUiScale();
    const float width = std::max(1.0f, std::min(640.0f * scale, viewport->Size.x - 32.0f * scale));
    ImGui::SetNextWindowPos(
        {viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + ImGui::GetFrameHeight() * 3.0f},
        ImGuiCond_Appearing, {0.5f, 0});
    ImGui::SetNextWindowSize({width, 0}, ImGuiCond_Appearing);
    if (!ImGui::BeginPopup(PaletteId,
                           ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        const bool closing = _wasOpen;
        _wasOpen = false;
        return opening || closing;
    }
    _wasOpen = true;
    ImGui::TextUnformatted("Commands");
    if (opening || _focusSearch) ImGui::SetKeyboardFocusHere();
    _focusSearch = false;
    ImGui::SetNextItemWidth(-1);
    const bool enter = ImGui::InputTextWithHint("##command-search", "Type a command...", _search.data(), _search.size(),
                                                ImGuiInputTextFlags_EnterReturnsTrue);
    const bool searchChanged = ImGui::IsItemEdited();
    if (searchChanged) _selected = 0;
    std::vector<PaletteEntry> entries;
    for (const auto& command : EditorCommands()) {
        const auto id = command.command;
        const char* shortcut =
            command.shortcutLabel ? command.shortcutLabel : (id == EditorPrimaryCommand(snapshot) ? "F5" : "");
        entries.push_back({std::string("Preview: ") + EditorCommandLabel(id, snapshot), shortcut,
                           snapshot.capabilities.Can(id), snapshot.capabilities.Reason(id),
                           [&, id] { actions.command(id); }});
    }
    entries.push_back({"Project: Find preview", "Ctrl+P", true, {}, [&] { result.focusCatalog = true; }});
    entries.push_back({maximized ? "View: Restore workspace" : "View: Focus preview", "", true, {}, [&] {
                           result.toggleMaximize = true;
                       }});
    entries.push_back({"Window: Toggle last bottom tool", "Ctrl+J", true, {}, [&] { result.toggleBottom = true; }});
    for (const auto& panel : EditorPanels) {
        const auto id = panel.id;
        entries.push_back({std::string("Window: ") + (preferences.Visible(id) ? "Hide " : "Show ") + panel.title,
                           "",
                           true,
                           {},
                           [&preferences, id] { preferences.Visible(id) = !preferences.Visible(id); }});
    }
    for (const auto& layout : EditorLayouts) {
        const auto id = layout.id;
        const bool enabled = id != EditorWorkspaceLayout::Custom || !preferences.customDocking.empty();
        entries.push_back({std::string("Window: Layouts / ") + layout.title, "", enabled,
                           enabled ? "" : "No custom layout has been saved", [&result, id] { result.layout = id; }});
    }
    std::erase_if(entries, [&](const auto& entry) { return !Matches(entry.label, _search.data()); });
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
        if (entries.empty()) ToolEmptyState("No matching commands", "Try a preview, panel or layout name.");
        for (int index = 0; index < count; ++index) {
            auto& entry = entries[static_cast<std::size_t>(index)];
            ImGui::PushID(index);
            ImGui::BeginDisabled(!entry.enabled);
            if (ImGui::Selectable(entry.label.c_str(), index == _selected, ImGuiSelectableFlags_DontClosePopups)) {
                _selected = index;
                execute = true;
            }
            if (index == _selected && moved) ImGui::SetScrollHereY();
            ImGui::EndDisabled();
            if (!entry.enabled)
                ToolTooltip(entry.reason.c_str());
            else if (entry.shortcut && *entry.shortcut)
                ToolTooltip(entry.shortcut);
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    if (count > 0) {
        const auto& selected = entries[static_cast<std::size_t>(_selected)];
        ImGui::TextDisabled("%s",
                            selected.enabled ? (selected.shortcut ? selected.shortcut : "") : selected.reason.c_str());
    }
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
