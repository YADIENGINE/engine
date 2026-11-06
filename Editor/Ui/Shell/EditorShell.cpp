#include "Editor/Ui/Shell/EditorShell.h"
#include <algorithm>
#include <array>
#include <format>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include "Editor/Ui/Commands/EditorActions.h"
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
constexpr const char* ShortcutsPopup = "Keyboard shortcuts###editor.shortcuts";
constexpr const char* AboutPopup = "About###editor.about";
bool BeginShortcutTable(const char* title) {
    ImGui::SeparatorText(title);
    if (!ImGui::BeginTable(title, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) return false;
    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed,
                            ImGui::CalcTextSize("Ctrl+PageDown / Ctrl+PageUp").x + ImGui::GetStyle().CellPadding.x * 2);
    return true;
}
void ShortcutRow(const char* action, const char* shortcut) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(action);
    ImGui::TableSetColumnIndex(1);
    ImGui::PushFont(ToolFont(ToolFontRole::Technical), ToolMetrics::CaptionSize);
    ImGui::TextUnformatted(shortcut);
    ImGui::PopFont();
}
// Centered and sized to the window (height 0: fits its content); closes with its button or Escape.
bool BeginHelpPopup(const char* id, bool open, float width, float height) {
    if (open) ImGui::OpenPopup(id);
    const auto* viewport = ImGui::GetMainViewport();
    const float scale = ToolUiScale();
    const float fitHeight =
        height > 0 ? std::max(1.0f, std::min(height * scale, viewport->Size.y - 32.0f * scale)) : 0.0f;
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({std::max(1.0f, std::min(width * scale, viewport->Size.x - 32.0f * scale)), fitHeight},
                             ImGuiCond_Appearing);
    return ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
}
void EndHelpPopup() {
    if (ToolButton("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}
// Every menu action with a shortcut, under its menu's name, then the keys no menu shows (the Agents drawer's).
bool KeyboardShortcuts(bool open, std::span<const EditorAction> list) {
    if (!BeginHelpPopup(ShortcutsPopup, open, 620.0f, 600.0f)) return false;
    ImGui::TextWrapped("Shortcuts available throughout the editor. Agent navigation shortcuts apply while the "
                       "Agents drawer is open.");
    ImGui::BeginChild("shortcut-list", {0, -ImGui::GetFrameHeightWithSpacing()});
    for (int index = 0; index < static_cast<int>(EditorMenu::Count); ++index) {
        const auto menu = static_cast<EditorMenu>(index);
        const auto shortcut = [menu](const EditorAction& action) {
            return action.menu == menu && action.inMenu && *action.shortcut;
        };
        if (std::ranges::none_of(list, shortcut) || !BeginShortcutTable(EditorMenuTitle(menu))) continue;
        for (const auto& action : list)
            if (shortcut(action)) ShortcutRow(action.label.c_str(), action.shortcut);
        ImGui::EndTable();
    }
    if (BeginShortcutTable("Agents drawer")) {
        for (const auto& binding : EditorKeyBindings())
            if (binding.name && *binding.name) ShortcutRow(binding.name, binding.text);
        ImGui::EndTable();
    }
    ImGui::EndChild();
    EndHelpPopup();
    return true;
}
std::string RepositoryPage() {
    std::string repository = ENGINE_REPOSITORY;
    if (repository.ends_with(".git")) repository.resize(repository.size() - 4);
    return repository;
}
bool About(bool open, const EditorUiActions& actions) {
    if (!BeginHelpPopup(AboutPopup, open, 420.0f, 0.0f)) return false;
    ImGui::PushFont(ToolFont(ToolFontRole::Medium), ToolMetrics::BodySize);
    ImGui::TextUnformatted(ENGINE_NAME " " ENGINE_VERSION);
    ImGui::PopFont();
    ImGui::TextDisabled("Engine, editor and tools share this version.");
    const auto repository = RepositoryPage();
    ImGui::TextDisabled("%s", repository.c_str());
    ImGui::Spacing();
    ImGui::BeginDisabled(!actions.openPath);
    if (ToolButton("Open repository")) actions.openPath(repository);
    ImGui::EndDisabled();
    ImGui::SameLine();
    EndHelpPopup();
    return true;
}
// One menu's items in list order: a separator where the section changes, submenus where they are named.
void DrawMenu(EditorMenu menu, std::span<const EditorAction> list) {
    std::string_view section;
    std::string_view submenu;
    bool submenuOpen = false;
    bool first = true;
    for (const auto& action : list) {
        if (action.menu != menu || !action.inMenu) continue;
        const bool newSection = !first && section != action.section;
        if (first || newSection || submenu != action.submenu) {
            if (submenuOpen) ImGui::EndMenu();
            submenuOpen = false;
            if (newSection) ImGui::Separator();
            section = action.section;
            submenu = action.submenu;
            if (!submenu.empty()) submenuOpen = ImGui::BeginMenu(action.submenu);
        } else if (submenuOpen && action.separatorBefore) {
            ImGui::Separator();
        }
        first = false;
        if (!submenu.empty() && !submenuOpen) continue;
        const char* help = action.enabled || action.reason.empty() ? action.tooltip.c_str() : action.reason.c_str();
        if (action.content) {
            if (ImGui::BeginMenu(action.label.c_str(), action.enabled)) {
                action.content();
                ImGui::EndMenu();
            }
        } else if (ImGui::MenuItem(action.label.c_str(), action.shortcut, action.checked, action.enabled) &&
                   action.run) {
            action.run();
        }
        if (*help) ToolTooltip(help);
    }
    if (submenuOpen) ImGui::EndMenu();
}
bool PhaseIsWorking(const EditorSnapshot& snapshot) {
    return snapshot.profiler.busy || snapshot.building || snapshot.phase == EditorPhase::Building ||
           snapshot.phase == EditorPhase::Starting || snapshot.phase == EditorPhase::ReadingCatalog;
}
// The phase as a square cell (or walking cells while working) and its name, aligned with the bar's buttons.
void StatusPhase(const EditorSnapshot& snapshot) {
    const char* phase = snapshot.profiler.busy ? (snapshot.benchmarkCancelPending ? "Canceling benchmark"
                                                                                  : snapshot.profiler.phase.c_str())
                        : snapshot.paused && snapshot.pausedForMarking ? "Paused for marking"
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
    ImGui::TextColored(PhaseIsWorking(snapshot) ? EditorInfoColor() : EditorPhaseColor(snapshot.phase), "%s", phase);
}
bool BuildFailed(const EditorSnapshot& snapshot) {
    const auto result = snapshot.diagnostics.Attempt().result;
    return result == EditorBuildResult::BuildFailed || result == EditorBuildResult::RefreshFailed;
}
} // namespace
void EditorShell::Menu(const EditorActionContext& context, std::span<const EditorAction> list,
                       const std::string& projectName, EditorGitBar& gitBar) const {
    auto& result = context.result;
    if (ImGui::BeginMainMenuBar()) {
        ToolPixelText("Y", 2, ToolPalette::Accent, ImGui::GetFrameHeight());
        ImGui::SameLine();
        for (int index = 0; index < static_cast<int>(EditorMenu::Count); ++index) {
            const auto menu = static_cast<EditorMenu>(index);
            if (std::ranges::none_of(
                    list, [menu](const EditorAction& action) { return action.menu == menu && action.inMenu; }))
                continue;
            if (!ImGui::BeginMenu(EditorMenuTitle(menu))) continue;
            DrawMenu(menu, list);
            ImGui::EndMenu();
        }
        const auto& snapshot = context.snapshot;
        if (snapshot.git) {
            const auto bar = gitBar.Draw(*snapshot.git, context.actions, projectName, snapshot.building);
            if (bar.toggle) result.git.toggle = bar.toggle;
            if (bar.show) result.git.show = bar.show;
        } else if (const float width = ImGui::CalcTextSize(projectName.c_str()).x;
                   !projectName.empty() && width < ImGui::GetContentRegionAvail().x) {
            ImGui::SameLine(ImGui::GetWindowWidth() - width - ImGui::GetStyle().WindowPadding.x * 2);
            ImGui::TextDisabled("%s", projectName.c_str());
        }
        ImGui::EndMainMenuBar();
    }
    // A request made in the menus this frame opens its window now; the palette's opens it next frame.
    const bool shortcuts = KeyboardShortcuts(std::exchange(result.showShortcuts, false), list);
    const bool about = About(std::exchange(result.showAbout, false), context.actions);
    result.dialogOpen = shortcuts || about;
}
float EditorShell::Toolbar(const EditorSnapshot& snapshot, const EditorUiActions& actions, ImVec2 position, float width,
                           bool& marking, EditorToolbarSearch& search, const EditorAgentStatus& agent,
                           EditorAgentActivity& activity, const std::string& markTarget) const {
    const float scale = ToolUiScale();
    const float height = std::max(EditorToolbarMetrics::ControlHeight * scale, ImGui::GetFrameHeight()) +
                         EditorToolbarMetrics::PaddingY * 2 * scale;
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize({width, height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        {EditorToolbarMetrics::PaddingX * scale, EditorToolbarMetrics::PaddingY * scale});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToolPalette::Window);
    if (ImGui::Begin("Toolbar###editor.toolbar", nullptr, BarFlags))
        DrawEditorPlaybackControls(snapshot, actions, marking, search, agent, activity, markTarget);
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    return height;
}
float EditorShell::Status(const EditorSnapshot& snapshot, const EditorPreferences& preferences,
                          const EditorAgentStatus& agent, ImVec2 position, float width, EditorShellResult& result,
                          const EditorUiActions& actions) const {
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
        if (snapshot.paused && snapshot.pausedForMarking) {
            ImGui::SameLine();
            EditorCommandButton(EditorCommand::Pause, snapshot, actions, true);
        }
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
