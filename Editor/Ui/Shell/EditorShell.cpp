#include "Editor/Ui/Shell/EditorShell.h"
#include <algorithm>
#include <string>
#include "Editor/Ui/Commands/EditorCommands.h"
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorPlaybackControls.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"

namespace {
constexpr ImGuiWindowFlags BarFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoScrollWithMouse;
void LanguageMenu(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    const PreviewParameter* language = nullptr;
    if (snapshot.selected) {
        const auto found = std::ranges::find(snapshot.selected->parameters, "language", &PreviewParameter::id);
        if (found != snapshot.selected->parameters.end() && found->kind == PreviewParameterKind::Choice)
            language = &*found;
    }
    if (!ImGui::BeginMenu("Language")) return;
    if (!language) {
        ImGui::TextDisabled("Select a preview with language support.");
        ImGui::EndMenu();
        return;
    }
    const bool enabled = !snapshot.busy && snapshot.parameters.contains(language->id) &&
                         std::ranges::find(snapshot.inspection.hiddenParameters, language->id) ==
                             snapshot.inspection.hiddenParameters.end() &&
                         std::ranges::find(snapshot.inspection.disabledParameters, language->id) ==
                             snapshot.inspection.disabledParameters.end();
    const auto found = snapshot.parameters.find(language->id);
    const auto* selected = found != snapshot.parameters.end() ? std::get_if<std::string>(&found->second) : nullptr;
    ImGui::BeginDisabled(!enabled);
    for (const auto& choice : language->choices)
        if (ImGui::MenuItem(choice.c_str(), nullptr, selected && *selected == choice))
            actions.parameter(language->id, choice);
    ImGui::EndDisabled();
    if (snapshot.pending.contains(language->id)) ImGui::TextDisabled("Pending");
    if (snapshot.busy)
        ImGui::TextDisabled("Wait for the current operation to finish.");
    else if (!snapshot.ready)
        ImGui::TextDisabled("Applies when the preview opens.");
    ImGui::EndMenu();
}
} // namespace
EditorShellResult EditorShell::Menu(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                    EditorPreferences& preferences, int& fontSize, bool maximized) const {
    EditorShellResult result;
    if (!ImGui::BeginMainMenuBar()) return result;
    if (ImGui::BeginMenu("File")) {
        result.focusCatalog = ImGui::MenuItem("Find preview", "Ctrl+P");
        EditorCommandMenuItem(EditorCommand::Open, snapshot, actions);
        ImGui::Separator();
        result.quit = ImGui::MenuItem("Exit");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Preview")) {
        EditorCommandMenuItem(EditorPrimaryCommand(snapshot), snapshot, actions);
        if (snapshot.profiler.busy) EditorCommandMenuItem(EditorCommand::Stop, snapshot, actions);
        EditorCommandMenuItem(EditorCommand::Pause, snapshot, actions);
        EditorCommandMenuItem(EditorCommand::Step, snapshot, actions);
        EditorCommandMenuItem(EditorCommand::Reset, snapshot, actions);
        ImGui::Separator();
        LanguageMenu(snapshot, actions);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        for (const auto& panel : EditorPanels)
            ImGui::MenuItem(panel.title, nullptr, &preferences.Visible(panel.id));
        ImGui::Separator();
        result.toggleMaximize = ImGui::MenuItem(maximized ? "Restore workspace" : "Focus preview");
        result.toggleBottom = ImGui::MenuItem("Toggle bottom tools", "Ctrl+J");
        if (ImGui::BeginMenu("Layout")) {
            for (const auto& layout : EditorLayouts)
                if (ImGui::MenuItem(layout.title, nullptr, preferences.layout == layout.id,
                                    layout.id != EditorWorkspaceLayout::Custom || !preferences.customDocking.empty()))
                    result.layout = layout.id;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Interface scale")) {
            for (const int percent : EditorFontSizes) {
                const auto label = std::to_string(percent) + "%";
                if (ImGui::MenuItem(label.c_str(), nullptr, fontSize == percent)) fontSize = percent;
            }
            ImGui::EndMenu();
        }
        ImGui::MenuItem("Preview performance metrics", nullptr, &preferences.showMetrics);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Tools")) {
        EditorCommandMenuItem(EditorCommand::Refresh, snapshot, actions);
        result.openCommands = ImGui::MenuItem("Command palette", "Ctrl+Shift+P");
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
    return result;
}
float EditorShell::Toolbar(const EditorSnapshot& snapshot, const EditorUiActions& actions, ImVec2 position,
                           float width) const {
    const float scale = EditorUiScale();
    const float height = std::max(32.0f * scale, ImGui::GetFrameHeight() + 4.0f * scale);
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize({width, height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.0f * scale, 2.0f * scale});
    if (ImGui::Begin("Toolbar###editor.toolbar", nullptr, BarFlags)) DrawEditorPlaybackControls(snapshot, actions);
    ImGui::End();
    ImGui::PopStyleVar();
    return height;
}
float EditorShell::Status(const EditorSnapshot& snapshot, const EditorPreferences& preferences, ImVec2 position,
                          float width, EditorShellResult& result) const {
    const float scale = EditorUiScale();
    const float height = std::max(24.0f * scale, ImGui::GetTextLineHeight() + 8.0f * scale);
    ImGui::SetNextWindowPos({position.x, position.y - height});
    ImGui::SetNextWindowSize({width, height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.0f * scale, 2.0f * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {6.0f * scale, 2.0f * scale});
    if (ImGui::Begin("Status###editor.status", nullptr, BarFlags)) {
        const auto errors = snapshot.diagnostics.UnreadErrors();
        const auto output =
            (errors ? "Output (" + std::to_string(errors) + ")" : std::string("Output")) + "###editor.output-status";
        if (EditorSegment(output.c_str(), preferences.visible[static_cast<std::size_t>(EditorPanel::Output)]))
            result.bottomPanel = EditorPanel::Output;
        if (errors) EditorTooltip("Unread errors. Open Output for details.");
        ImGui::SameLine();
        if (EditorSegment("Profiler", preferences.visible[static_cast<std::size_t>(EditorPanel::Profiler)]))
            result.bottomPanel = EditorPanel::Profiler;
        ImGui::SameLine();
        const char* phase = snapshot.profiler.busy ? (snapshot.benchmarkCancelPending ? "Canceling benchmark"
                                                                                      : snapshot.profiler.phase.c_str())
                                                   : EditorPhaseLabel(snapshot.phase);
        EditorStatusBadge(phase, EditorPhaseColor(snapshot.profiler.busy ? EditorPhase::Starting : snapshot.phase));
        ImGui::SameLine();
        const auto cursor = ImGui::GetCursorScreenPos();
        const auto available = ImGui::GetContentRegionAvail();
        if (available.x > ImGui::GetFontSize() * 3.0f) {
            ImGui::InvisibleButton("status-detail", {available.x, ImGui::GetTextLineHeight()});
            auto* draw = ImGui::GetWindowDrawList();
            draw->PushClipRect(cursor, {cursor.x + available.x, cursor.y + ImGui::GetTextLineHeight()}, true);
            draw->AddText(cursor, ImGui::GetColorU32(ImGuiCol_TextDisabled), snapshot.status.c_str());
            draw->PopClipRect();
            EditorTooltip(snapshot.status.c_str());
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    return height;
}
