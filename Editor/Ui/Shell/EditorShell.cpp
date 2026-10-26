#include "Editor/Ui/Shell/EditorShell.h"
#include <algorithm>
#include <array>
#include <string>
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"

namespace {
constexpr ImGuiWindowFlags BarFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                      ImGuiWindowFlags_NoBringToFrontOnFocus;
void CommandItem(const char* label, EditorCommand command, const EditorSnapshot& snapshot,
                 const EditorUiActions& actions) {
    if (ImGui::MenuItem(label, nullptr, false, snapshot.capabilities.Can(command))) actions.command(command);
}
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
    for (const auto& choice : language->choices) {
        if (ImGui::MenuItem(choice.c_str(), nullptr, selected && *selected == choice))
            actions.parameter(language->id, choice);
    }
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
        CommandItem("Open selected preview", EditorCommand::Open, snapshot, actions);
        ImGui::Separator();
        CommandItem("Build & Refresh", EditorCommand::Refresh, snapshot, actions);
        CommandItem("Cancel operation", EditorCommand::Cancel, snapshot, actions);
        ImGui::Separator();
        result.quit = ImGui::MenuItem("Exit");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Options")) {
        LanguageMenu(snapshot, actions);
        ImGui::Separator();
        ImGui::TextDisabled("Font size");
        bool first = true;
        for (const int percent : EditorFontSizes) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(percent == fontSize ? ImGuiCol_HeaderActive
                                                                                                : ImGuiCol_Button));
            const auto label = std::to_string(percent) + "%";
            if (!first) ContinueEditorToolbar(label.c_str());
            if (ImGui::Button(label.c_str())) fontSize = percent;
            ImGui::PopStyleColor();
            first = false;
        }
        ImGui::Separator();
        ImGui::MenuItem("Preview performance metrics", nullptr, &preferences.showMetrics);
        ImGui::MenuItem("Follow output", nullptr, &preferences.followOutput);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Windows")) {
        constexpr std::array Names{"Project", "View", "Inspector", "Output"};
        const auto unread = snapshot.diagnostics.UnreadErrors();
        for (std::size_t i = 0; i < Names.size(); ++i) {
            const auto label = i == static_cast<std::size_t>(EditorPanel::Output) && unread
                                   ? std::string(Names[i]) + " (" + std::to_string(unread) + " errors)"
                                   : std::string(Names[i]);
            ImGui::MenuItem(label.c_str(), nullptr, &preferences.visible[i]);
        }
        ImGui::Separator();
        result.toggleMaximize = ImGui::MenuItem(maximized ? "Restore workspace" : "Focus preview");
        result.restoreLayout = ImGui::MenuItem("Restore default layout");
        ImGui::EndMenu();
    }
    if (snapshot.diagnostics.UnreadErrors()) {
        ImGui::TextColored(EditorPhaseColor(EditorPhase::Error), "%zu errors", snapshot.diagnostics.UnreadErrors());
    }
    ImGui::EndMainMenuBar();
    return result;
}
float EditorShell::Status(const EditorSnapshot& snapshot, ImVec2 position, float width) const {
    const auto& style = ImGui::GetStyle();
    const auto* phase = EditorPhaseLabel(snapshot.phase);
    const auto status = "| " + snapshot.status;
    const float contentWidth = std::max(1.0f, width - style.WindowPadding.x * 2);
    const bool wrap =
        ImGui::CalcTextSize(phase).x + style.ItemSpacing.x + ImGui::CalcTextSize(status.c_str()).x > contentWidth;
    const float textHeight =
        ImGui::GetTextLineHeight() +
        (wrap ? style.ItemSpacing.y + ImGui::CalcTextSize(status.c_str(), nullptr, false, contentWidth).y : 0.0f);
    const float height = std::max(ImGui::GetFrameHeight(), textHeight) + style.WindowPadding.y * 2;
    ImGui::SetNextWindowPos({position.x, position.y - height});
    ImGui::SetNextWindowSize({width, height});
    ImGui::Begin("Status###editor.status", nullptr, BarFlags);
    ImGui::TextColored(EditorPhaseColor(snapshot.phase), "%s", phase);
    if (!wrap) ImGui::SameLine();
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("%s", status.c_str());
    ImGui::PopTextWrapPos();
    EditorTooltip(snapshot.status.c_str());
    ImGui::End();
    return height;
}
