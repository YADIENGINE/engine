#include "Editor/Ui/Shell/EditorShell.h"
#include <algorithm>
#include <format>
#include <string>
#include "Editor/Ui/Commands/EditorCommands.h"
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorPlaybackControls.h"
#include "Editor/Ui/Widgets/EditorAgentSetup.h"
#include "ToolUi/ToolPixel.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr ImGuiWindowFlags BarFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoScrollWithMouse;
constexpr float StatusGap = 10.0f;
bool PhaseIsWorking(const EditorSnapshot& snapshot) {
    return snapshot.profiler.busy || snapshot.building || snapshot.phase == EditorPhase::Building ||
           snapshot.phase == EditorPhase::Starting || snapshot.phase == EditorPhase::ReadingCatalog;
}
// The phase as a square cell (or walking cells while working) and its name, aligned with the bar's buttons.
void StatusPhase(const EditorSnapshot& snapshot) {
    const char* phase = snapshot.profiler.busy ? (snapshot.benchmarkCancelPending ? "Canceling benchmark"
                                                                                  : snapshot.profiler.phase.c_str())
                                               : EditorPhaseLabel(snapshot.phase);
    const float y = ImGui::GetCursorPosY();
    ImGui::SetCursorPosY(y + ImGui::GetStyle().FramePadding.y);
    if (PhaseIsWorking(snapshot))
        ToolActivity();
    else
        ToolStateCell(EditorPhaseColor(snapshot.phase));
    ImGui::SameLine();
    ImGui::SetCursorPosY(y);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(phase);
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
                    : "Run a scene before sending its context.");
    const bool failed = BuildFailed(snapshot);
    if (ImGui::MenuItem("Send build failure", nullptr, false, failed))
        result.agent = EditorAgentRequest::SendBuildFailure;
    ToolTooltip(failed ? "Paste the last build failure into the agent session, without Enter."
                       : "The last Build & Refresh did not fail.");
    if (ImGui::MenuItem("Ask agent to fix build", nullptr, false, failed)) result.agent = EditorAgentRequest::FixBuild;
    ToolTooltip(failed ? "Send the build errors and ask the agent to fix them. This presses Enter."
                       : "The last Build & Refresh did not fail.");
    if (ImGui::MenuItem("Show activity")) result.showActivity = true;
    ToolTooltip("What agent tools changed or captured in this session, with Undo.");
    ImGui::SeparatorText("Settings");
    (void)ToolToggle("Agent control", agent.control);
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
                                    bool offerProjectList, const std::string& projectName) const {
    EditorShellResult result;
    if (!ImGui::BeginMainMenuBar()) return result;
    ToolPixelText("Y", 2, ToolPalette::Accent, ImGui::GetFrameHeight());
    ImGui::SameLine();
    if (ImGui::BeginMenu("File")) {
        result.openQuick = ImGui::MenuItem("Quick open", "Ctrl+P");
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
        EditorCommandMenuItem(EditorCommand::BuildAndPlay, snapshot, actions);
        result.openCommands = ImGui::MenuItem("Command palette", "Ctrl+Shift+P");
        ImGui::EndMenu();
    }
    AgentMenu(snapshot, preferences.agent, result);
    if (const float width = ImGui::CalcTextSize(projectName.c_str()).x;
        !projectName.empty() && width < ImGui::GetContentRegionAvail().x) {
        ImGui::SameLine(ImGui::GetWindowWidth() - width - ImGui::GetStyle().WindowPadding.x * 2);
        ImGui::TextDisabled("%s", projectName.c_str());
    }
    ImGui::EndMainMenuBar();
    return result;
}
float EditorShell::Toolbar(const EditorSnapshot& snapshot, const EditorUiActions& actions, ImVec2 position, float width,
                           bool& marking, EditorToolbarSearch& search, const EditorAgentStatus& agent,
                           EditorAgentActivity& activity) const {
    const float scale = ToolUiScale();
    const float height = std::max(EditorToolbarMetrics::ControlHeight * scale, ImGui::GetFrameHeight()) +
                         EditorToolbarMetrics::PaddingY * 2 * scale;
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize({width, height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        {EditorToolbarMetrics::PaddingX * scale, EditorToolbarMetrics::PaddingY * scale});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToolPalette::Window);
    if (ImGui::Begin("Toolbar###editor.toolbar", nullptr, BarFlags))
        DrawEditorPlaybackControls(snapshot, actions, marking, search, agent, activity);
    ImGui::End();
    ImGui::PopStyleColor();
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
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToolPalette::Window);
    if (ImGui::Begin("Status###editor.status", nullptr, BarFlags)) {
        const auto errors = snapshot.diagnostics.UnreadErrors();
        const auto warnings = snapshot.diagnostics.UnreadWarnings();
        const auto unread = errors + warnings;
        const auto output =
            (unread ? "Output (" + std::to_string(unread) + ")" : std::string("Output")) + "###editor.output-status";
        const bool outputVisible = preferences.visible[static_cast<std::size_t>(EditorPanel::Output)];
        const float gap = StatusGap * scale;
        const auto base = ToolSurfaceColor(ToolSurface::Status);
        const ImVec4 outputAttention =
            unread ? EditorTintSurface(base, EditorLevelColor(errors ? EditorLogLevel::Error : EditorLogLevel::Warning),
                                       EditorAttentionPulse())
                   : ImVec4{0, 0, 0, 0};
        if (ToolSegment(output.c_str(), outputVisible, outputAttention)) result.bottomPanel = EditorPanel::Output;
        if (unread) ToolTooltip("Unread errors or warnings. Open Output for details.");
        ImGui::SameLine();
        if (ToolSegment("Console", preferences.visible[static_cast<std::size_t>(EditorPanel::Console)]))
            result.bottomPanel = EditorPanel::Console;
        ToolTooltip("Terminal in the project folder (Ctrl+`)");
        ImGui::SameLine();
        if (const auto buildErrors = snapshot.diagnostics.ProblemErrors(); buildErrors > 0 && BuildFailed(snapshot)) {
            const auto tone = EditorLevelColor(EditorLogLevel::Error);
            const auto label =
                std::format("{} error{}###editor.problems-status", buildErrors, buildErrors == 1 ? "" : "s");
            ImGui::PushStyleColor(ImGuiCol_TextDisabled, tone);
            if (ToolSegment(label.c_str(), false, EditorTintSurface(base, tone, 0.14f))) result.showProblems = true;
            ImGui::PopStyleColor();
            ToolTooltip("The last build failed. Click to see its problems.");
            ImGui::SameLine();
        }
        if (!agent.control) {
            ToolStatusBadge("Agent control off", EditorLevelColor(EditorLogLevel::Warning));
            ToolTooltip("Agent tools can observe the editor, but commands that change it are refused.");
            ImGui::SameLine();
        }
        // After the buttons, so they stay in place while the phase's name changes.
        ImGui::SameLine(0, gap);
        StatusPhase(snapshot);
        ImGui::SameLine(0, gap);
        // The scene's size and rate at the far end, the status message in what is left.
        std::string metrics;
        if (snapshot.ready)
            metrics = std::to_string(snapshot.canvas.x) + "\xC3\x97" + std::to_string(snapshot.canvas.y) +
                      "  \xC2\xB7  " + std::to_string(static_cast<int>(snapshot.streamFps + 0.5)) + " fps";
        const float rowHeight = ImGui::GetFrameHeight();
        ImGui::PushFont(ToolFont(ToolFontRole::Ui), ToolMetrics::CaptionSize);
        const float metricsWidth = metrics.empty() ? 0.0f : ImGui::CalcTextSize(metrics.c_str()).x + gap;
        const auto cursor = ImGui::GetCursorScreenPos();
        const auto available = ImGui::GetContentRegionAvail();
        const float detailWidth = available.x - metricsWidth;
        const float textY = cursor.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;
        auto* draw = ImGui::GetWindowDrawList();
        if (detailWidth > ImGui::GetFontSize() * 3.0f) {
            ImGui::InvisibleButton("status-detail", {detailWidth, rowHeight});
            // While building: how far it got, from Ninja's own count.
            const auto& progress = snapshot.diagnostics.Progress();
            const bool counting = (snapshot.building || snapshot.phase == EditorPhase::Building) && progress;
            const auto text = counting ? std::format("Building {}/{}  \xC2\xB7  {:.0f} s", progress->done,
                                                     progress->total, snapshot.diagnostics.AttemptSeconds())
                                       : snapshot.status;
            const auto detail = ToolClipText(text.c_str(), detailWidth - gap);
            draw->AddText({cursor.x + gap * 0.5f, textY}, ImGui::GetColorU32(ToolPalette::Muted), detail.c_str());
            ToolTooltip(text.c_str());
        }
        if (!metrics.empty() && metricsWidth < available.x)
            draw->AddText({cursor.x + available.x - metricsWidth + gap, textY}, ImGui::GetColorU32(ToolPalette::Muted),
                          metrics.c_str());
        ImGui::PopFont();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    return height;
}
