#include "Editor/Ui/Shell/EditorShell.h"
#include <algorithm>
#include <string>
#include "Editor/Ui/Commands/EditorCommands.h"
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorPlaybackControls.h"
#include "Editor/Ui/Widgets/EditorAgentSetup.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr ImGuiWindowFlags BarFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoScrollWithMouse;
ImVec4 TintSurface(ImVec4 base, ImVec4 tone, float amount) {
    return {base.x + (tone.x - base.x) * amount, base.y + (tone.y - base.y) * amount,
            base.z + (tone.z - base.z) * amount, base.w};
}
void LanguageMenu(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    const SceneParameter* language = nullptr;
    if (snapshot.selected) {
        const auto found = std::ranges::find(snapshot.selected->parameters, "language", &SceneParameter::id);
        if (found != snapshot.selected->parameters.end() && found->kind == SceneParameterKind::Choice)
            language = &*found;
    }
    if (!ImGui::BeginMenu("Language")) return;
    if (!language) {
        ImGui::TextDisabled("Select a scene with language support.");
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
        ImGui::TextDisabled("Applies when the scene opens.");
    ImGui::EndMenu();
}
bool BuildFailed(const EditorSnapshot& snapshot) {
    const auto result = snapshot.diagnostics.Attempt().result;
    return result == EditorBuildResult::BuildFailed || result == EditorBuildResult::RefreshFailed;
}
void AgentMenu(const EditorSnapshot& snapshot, AgentPreferences& agent, EditorShellResult& result) {
    if (!ImGui::BeginMenu("Agent")) return;
    ImGui::SeparatorText("Send context");
    if (ImGui::MenuItem("Send scene", nullptr, false, snapshot.ready)) result.agent = EditorAgentRequest::SendScene;
    ToolTooltip(snapshot.ready
                    ? "Paste the open scene, its time, changed values and a screenshot into the agent session."
                    : "Open a scene before sending its context.");
    const bool failed = BuildFailed(snapshot);
    if (ImGui::MenuItem("Send build failure", nullptr, false, failed))
        result.agent = EditorAgentRequest::SendBuildFailure;
    ToolTooltip(failed ? "Paste the last build failure into the agent session, without Enter."
                       : "The last Build & Refresh did not fail.");
    if (ImGui::MenuItem("Ask agent to fix build", nullptr, false, failed)) result.agent = EditorAgentRequest::FixBuild;
    ToolTooltip(failed ? "Send the build errors and ask the agent to fix them. This presses Enter."
                       : "The last Build & Refresh did not fail.");
    ImGui::SeparatorText("Settings");
    ImGui::Checkbox("Agent control", &agent.control);
    ToolTooltip("Allow agent tools (python Engine ctl) to change the editor.\n"
                "Off: tools can still observe status, logs, parameters and screenshots.");
    if (ImGui::BeginMenu("Agent CLI")) {
        EditorAgentChoice(agent, ImGui::GetFontSize() * 26);
        ImGui::EndMenu();
    }
    ImGui::EndMenu();
}
} // namespace
EditorShellResult EditorShell::Menu(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                    EditorPreferences& preferences, int& fontSize, bool maximized,
                                    bool offerProjectList) const {
    EditorShellResult result;
    if (!ImGui::BeginMainMenuBar()) return result;
    if (ImGui::BeginMenu("File")) {
        result.focusCatalog = ImGui::MenuItem("Find scene", "Ctrl+P");
        EditorCommandMenuItem(EditorCommand::Open, snapshot, actions);
        ImGui::Separator();
        // Only when a launcher started this editor (ENGINE_LAUNCHER; Docs/ToolsContract.md).
        if (offerProjectList) result.returnToProjectList = ImGui::MenuItem("Back to project list");
        result.quit = ImGui::MenuItem("Exit");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Scene")) {
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
        result.toggleMaximize = ImGui::MenuItem(maximized ? "Restore workspace" : "Focus viewport");
        ImGui::Separator();
        if (ImGui::BeginMenu("Interface scale")) {
            for (const int percent : EditorFontSizes) {
                const auto label = std::to_string(percent) + "%";
                if (ImGui::MenuItem(label.c_str(), nullptr, fontSize == percent)) fontSize = percent;
            }
            ImGui::EndMenu();
        }
        ImGui::MenuItem("Viewport performance metrics", nullptr, &preferences.showMetrics);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")) {
        for (const auto& panel : EditorPanels)
            ImGui::MenuItem(panel.title, nullptr, &preferences.Visible(panel.id));
        ImGui::Separator();
        if (ImGui::BeginMenu("Layouts")) {
            for (const auto& layout : EditorLayouts)
                if (ImGui::MenuItem(layout.title, nullptr, preferences.layout == layout.id,
                                    layout.id != EditorWorkspaceLayout::Custom || !preferences.customDocking.empty()))
                    result.layout = layout.id;
            ImGui::EndMenu();
        }
        ImGui::Separator();
        result.toggleBottom = ImGui::MenuItem("Toggle last bottom tool", "Ctrl+J");
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Tools")) {
        EditorCommandMenuItem(EditorCommand::Refresh, snapshot, actions);
        result.openCommands = ImGui::MenuItem("Command palette", "Ctrl+Shift+P");
        ImGui::EndMenu();
    }
    AgentMenu(snapshot, preferences.agent, result);
    ImGui::EndMainMenuBar();
    return result;
}
float EditorShell::Toolbar(const EditorSnapshot& snapshot, const EditorUiActions& actions, ImVec2 position, float width,
                           bool& marking) const {
    const float scale = ToolUiScale();
    const float height = std::max(EditorToolbarMetrics::ControlHeight * scale, ImGui::GetFrameHeight()) +
                         EditorToolbarMetrics::PaddingY * 2 * scale;
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize({width, height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        {EditorToolbarMetrics::PaddingX * scale, EditorToolbarMetrics::PaddingY * scale});
    if (ImGui::Begin("Toolbar###editor.toolbar", nullptr, BarFlags))
        DrawEditorPlaybackControls(snapshot, actions, marking);
    ImGui::End();
    ImGui::PopStyleVar();
    return height;
}
float EditorShell::Status(const EditorSnapshot& snapshot, const EditorPreferences& preferences,
                          const EditorAgentStatus& agent, ImVec2 position, float width,
                          EditorShellResult& result) const {
    const float scale = ToolUiScale();
    const float height = std::max(24.0f * scale, ImGui::GetTextLineHeight() + 8.0f * scale);
    ImGui::SetNextWindowPos({position.x, position.y - height});
    ImGui::SetNextWindowSize({width, height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.0f * scale, 2.0f * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {6.0f * scale, 2.0f * scale});
    if (ImGui::Begin("Status###editor.status", nullptr, BarFlags)) {
        const auto errors = snapshot.diagnostics.UnreadErrors();
        const auto warnings = snapshot.diagnostics.UnreadWarnings();
        const auto unread = errors + warnings;
        const auto output =
            (unread ? "Output (" + std::to_string(unread) + ")" : std::string("Output")) + "###editor.output-status";
        const bool outputVisible = preferences.visible[static_cast<std::size_t>(EditorPanel::Output)];
        if (unread) {
            const auto tone = EditorLevelColor(errors ? EditorLogLevel::Error : EditorLogLevel::Warning);
            const auto base = ImGui::GetStyleColorVec4(outputVisible ? ImGuiCol_TabSelected : ImGuiCol_Tab);
            const auto background = TintSurface(base, tone, EditorAttentionPulse());
            ImGui::PushStyleColor(ImGuiCol_Tab, background);
            ImGui::PushStyleColor(ImGuiCol_TabSelected, background);
        }
        if (ToolSegment(output.c_str(), outputVisible)) result.bottomPanel = EditorPanel::Output;
        if (unread) {
            ImGui::PopStyleColor(2);
            ToolTooltip("Unread errors or warnings. Open Output for details.");
        }
        ImGui::SameLine();
        if (ToolSegment("Console", preferences.visible[static_cast<std::size_t>(EditorPanel::Console)]))
            result.bottomPanel = EditorPanel::Console;
        ToolTooltip("Terminal in the project folder (Ctrl+`)");
        ImGui::SameLine();
        if (agent.session) {
            const bool waiting = agent.state == SessionState::Waiting;
            const auto* icon = EditorSessionStateIcon(agent.state);
            const auto label = (icon[0] ? std::string(icon) + " " : std::string()) + agent.title + ": " +
                               SessionStateName(agent.state) + "###editor.agent-status";
            const auto tone = EditorSessionStateColor(agent.state);
            ImGui::PushStyleColor(ImGuiCol_Tab, TintSurface(ToolSurfaceColor(ToolSurface::Status), tone,
                                                            waiting ? EditorAttentionPulse() : 0.12f));
            ImGui::PushStyleColor(ImGuiCol_TextDisabled, tone);
            if (ToolSegment(label.c_str(), false)) result.agent = EditorAgentRequest::Open;
            ImGui::PopStyleColor(2);
            const auto detail =
                (agent.activity.empty() ? std::string() : agent.activity + "\n\n") + "Click to show the agent session";
            ToolTooltip(detail.c_str());
            ImGui::SameLine();
        }
        if (!agent.control) {
            ToolStatusBadge("Agent control off", EditorLevelColor(EditorLogLevel::Warning));
            ToolTooltip("Agent tools can observe the editor, but commands that change it are refused.");
            ImGui::SameLine();
        }
        const char* phase = snapshot.profiler.busy ? (snapshot.benchmarkCancelPending ? "Canceling benchmark"
                                                                                      : snapshot.profiler.phase.c_str())
                                                   : EditorPhaseLabel(snapshot.phase);
        ToolStatusBadge(phase, EditorPhaseColor(snapshot.profiler.busy ? EditorPhase::Starting : snapshot.phase));
        ImGui::SameLine();
        const auto cursor = ImGui::GetCursorScreenPos();
        const auto available = ImGui::GetContentRegionAvail();
        if (available.x > ImGui::GetFontSize() * 3.0f) {
            ImGui::InvisibleButton("status-detail", {available.x, ImGui::GetTextLineHeight()});
            auto* draw = ImGui::GetWindowDrawList();
            draw->PushClipRect(cursor, {cursor.x + available.x, cursor.y + ImGui::GetTextLineHeight()}, true);
            draw->AddText(cursor, ImGui::GetColorU32(ImGuiCol_TextDisabled), snapshot.status.c_str());
            draw->PopClipRect();
            ToolTooltip(snapshot.status.c_str());
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    return height;
}
