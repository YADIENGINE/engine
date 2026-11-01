#include "Editor/Ui/Workspace/EditorWorkspace.h"
#include <imgui_internal.h>
#include <algorithm>
#include <string_view>
#include "Editor/Internals/Platform/Windows/CommandLocator.h"
#include "Editor/Internals/Session/AgentContext.h"
#include "Editor/Internals/Session/ViewportMark.h"
#include "Editor/Ui/Commands/EditorCommands.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr float MarkOverlayTint = 0.06f;  // Alpha of the tint over the editor while marking
constexpr float MarkOutline = 2.0f;       // Pixels at 100% UI scale
constexpr float AudioColumnRatio = 0.34f; // Of the bottom dock, right of Assets
constexpr float AgentColumnRatio = 0.40f; // 60% Viewport / 40% Console + Inspector at desktop sizes.
constexpr float PanelMinWidthInFonts = 12.0f;
constexpr float PanelMinHeightInLines = 6.0f;
constexpr ImGuiWindowFlags HostFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
ImVec2 PanelMinimumSize(ImVec2 workspaceSize) {
    return {std::max(1.0f, std::min(ImGui::GetFontSize() * PanelMinWidthInFonts, workspaceSize.x * 0.3f)),
            std::max(1.0f,
                     std::min(ImGui::GetTextLineHeightWithSpacing() * PanelMinHeightInLines, workspaceSize.y * 0.5f))};
}
void SelectPanelTab(const char* name) {
    const auto* window = ImGui::FindWindowByName(name);
    if (!window || !window->DockNode) return;
    auto* node = window->DockNode;
    node->SelectedTabId = window->TabId;
    if (auto* tabs = node->TabBar) tabs->SelectedTabId = tabs->NextSelectedTabId = window->TabId;
}
ImGuiID SavedPanelDock(const EditorPanelDefinition& panel) {
    if (const auto* window = ImGui::FindWindowByName(panel.window)) return window->DockId;
    if (const auto* settings = ImGui::FindWindowSettingsByID(ImHashStr(panel.window))) return settings->DockId;
    return 0;
}
bool BottomVisible(const EditorPreferences& preferences) {
    for (const auto panel : EditorBottomPanels)
        if (preferences.visible[static_cast<std::size_t>(panel)]) return true;
    return false;
}
} // namespace
void EditorWorkspace::CaptureLayout(EditorPreferences& preferences) const {
    if (!_initialized || _maximized) return;
    std::size_t size = 0;
    const char* bytes = ImGui::SaveIniSettingsToMemory(&size);
    if (_ignoreLayoutChanges == 0 && !preferences.docking.empty() &&
        preferences.docking != std::string_view(bytes, size))
        preferences.layout = EditorWorkspaceLayout::Custom;
    preferences.docking.assign(bytes, size);
    if (preferences.layout == EditorWorkspaceLayout::Custom) {
        preferences.customDocking = preferences.docking;
        preferences.customVisible = preferences.visible;
    }
    ImGui::GetIO().WantSaveIniSettings = false;
}
void EditorWorkspace::SetMaximized(bool maximized, EditorPreferences& preferences) {
    if (maximized == _maximized) return;
    _viewport.ReleaseFocus();
    if (maximized) {
        CaptureLayout(preferences);
        _restoreLayout = preferences.docking;
    } else if (!_restoreLayout.empty()) {
        ImGui::LoadIniSettingsFromMemory(_restoreLayout.data(), _restoreLayout.size());
        _restoreLayout.clear();
    }
    _maximized = maximized;
    _ignoreLayoutChanges = 3;
}
void EditorWorkspace::ApplyLayout(EditorWorkspaceLayout layout, EditorPreferences& preferences) {
    if (layout == EditorWorkspaceLayout::Custom && preferences.customDocking.empty()) return;
    SetMaximized(false, preferences);
    CaptureLayout(preferences);
    if (preferences.customDocking.empty()) {
        preferences.customDocking = preferences.docking;
        preferences.customVisible = preferences.visible;
    }
    preferences.layout = layout;
    if (layout == EditorWorkspaceLayout::Custom) {
        _dockOnOpen.fill(false);
        preferences.docking = preferences.customDocking;
        preferences.visible = preferences.customVisible;
        ImGui::LoadIniSettingsFromMemory(preferences.docking.data(), preferences.docking.size());
    } else {
        preferences.visible = DefaultEditorPanelVisibility();
        if (layout == EditorWorkspaceLayout::Inspect) preferences.Visible(EditorPanel::Catalog) = false;
        if (layout == EditorWorkspaceLayout::Profile) {
            preferences.Visible(EditorPanel::Inspector) = false;
            preferences.Visible(EditorPanel::Tuning) = false;
            preferences.Visible(EditorPanel::Profiler) = true;
            preferences.lastBottomPanel = EditorPanel::Profiler;
        }
        if (layout == EditorWorkspaceLayout::Agent) {
            preferences.Visible(EditorPanel::Catalog) = false;
            preferences.Visible(EditorPanel::Tuning) = false;
            preferences.Visible(EditorPanel::Console) = true;
            preferences.lastBottomPanel = EditorPanel::Console;
        }
        _resetDock = true;
    }
    _ignoreLayoutChanges = 3;
    _viewport.ReleaseFocus();
}
void EditorWorkspace::MarkOverlay(const EditorSnapshot& snapshot, const EditorUiActions& actions, float top,
                                  EditorWorkspaceResult& result) {
    const auto* main = ImGui::GetMainViewport();
    const ImVec2 origin{main->Pos.x, main->Pos.y + top};
    const ImVec2 size{main->Size.x, std::max(1.0f, main->Size.y - top)};
    const auto& io = ImGui::GetIO();
    const auto& viewport = result.interaction.viewport;
    const auto onViewport = [&](ImVec2 point) {
        return snapshot.ready && MapViewportPoint(viewport, {point.x, point.y}).has_value();
    };
    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowFocus(); // Above every docked panel while marking
    constexpr ImGuiWindowFlags OverlayFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                                              ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove |
                                              ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    if (ImGui::Begin("##mark-overlay", nullptr, OverlayFlags)) {
        ImGui::InvisibleButton("mark-area", size);
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y},
                            ImGui::GetColorU32(ImGuiCol_DragDropTarget, MarkOverlayTint));
        const char* hint = "Drag (or click) to mark a region for the Console - Esc cancels";
        const ImVec2 hintSize = ImGui::CalcTextSize(hint);
        draw->AddText({origin.x + (size.x - hintSize.x) * 0.5f, origin.y + ImGui::GetStyle().WindowPadding.y},
                      ImGui::GetColorU32(ImGuiCol_Text), hint);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            _overlayStart = io.MousePos;
            // On the viewport the frame holds still, as with Alt+drag
            if (onViewport(io.MousePos) && !snapshot.paused && snapshot.capabilities.Can(EditorCommand::Pause))
                actions.command(EditorCommand::Pause);
        }
        if (_overlayStart) {
            const ImVec2 from{std::min(_overlayStart->x, io.MousePos.x), std::min(_overlayStart->y, io.MousePos.y)};
            const ImVec2 to{std::max(_overlayStart->x, io.MousePos.x), std::max(_overlayStart->y, io.MousePos.y)};
            draw->AddRect(from, to, ImGui::GetColorU32(ImGuiCol_DragDropTarget), 0.0f, 0, MarkOutline * ToolUiScale());
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                const ImVec2 start = *_overlayStart, end = io.MousePos;
                _overlayStart.reset();
                _marking = false;
                const bool inside = onViewport(start) && onViewport(end);
                if (inside) // Entirely on the viewport: its pixels and context
                    result.interaction.mark = MarkRegion(viewport, {start.x, start.y}, {end.x, end.y});
                else
                    result.windowMark = WindowMarkRegion({start.x, start.y}, {end.x, end.y}, {main->Pos.x, main->Pos.y},
                                                         {main->Size.x, main->Size.y},
                                                         {io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y});
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            _overlayStart.reset();
            _marking = false;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    result.cancelViewportInput = true;
}
void EditorWorkspace::ShowConsole(EditorPreferences& preferences) {
    if (_maximized) SetMaximized(false, preferences);
    preferences.Visible(EditorPanel::Console) = true;
    preferences.lastBottomPanel = EditorPanel::Console;
    _focusBottom = EditorPanel::Console;
    _viewport.ReleaseFocus();
}
void EditorWorkspace::PasteToConsole(const std::string& text, EditorPreferences& preferences) {
    ShowConsole(preferences);
    if (const auto agent = _console.AgentSession()) _console.Select(*agent);
    _console.Paste(text);
}
std::string EditorWorkspace::OpenAgent(EditorPreferences& preferences) {
    if (_agentSetup.Active()) return {};
    const auto& kind = AgentKindInfo(preferences.agent.kind);
    const auto command = AgentCommand(preferences.agent);
    const bool available = CommandOnPath(AgentProgram(command));
    if (!preferences.agent.configured || !available) {
        _agentSetup.Open(preferences.agent);
        _viewport.ReleaseFocus();
        if (!available)
            return std::string(kind.name) + ": '" + AgentProgram(command) +
                   "' was not found on PATH; choose an available command in agent setup";
        return {};
    }
    ShowConsole(preferences);
    // The agent session requests go to is reused when it runs the chosen CLI; otherwise a new session starts.
    const auto commandLine = AgentCommandLine(DefaultTerminalShell(), command);
    if (const auto agent = _console.AgentSession(); agent && _console.Sessions()[*agent].agent == kind.id)
        _console.Revive(*agent, commandLine);
    else
        _console.AddAgent(kind.id, commandLine);
    return {};
}
std::string EditorWorkspace::AddSession(const ConsoleNewSession& request, EditorPreferences& preferences) {
    if (!request.agent) {
        ShowConsole(preferences);
        _console.OpenShell();
        return {};
    }
    if (_agentSetup.Active()) return {};
    preferences.agent.kind = *request.agent; // The last agent added becomes the one agent requests start
    const auto command = AgentCommand(preferences.agent);
    if (!CommandOnPath(AgentProgram(command))) {
        _agentSetup.Open(preferences.agent);
        _viewport.ReleaseFocus();
        return std::string(AgentKindInfo(*request.agent).name) + ": '" + AgentProgram(command) +
               "' was not found on PATH; choose an available command in agent setup";
    }
    preferences.agent.configured = true;
    ShowConsole(preferences);
    _console.AddAgent(*request.agent, AgentCommandLine(DefaultTerminalShell(), command));
    return {};
}
std::string EditorWorkspace::SendToAgent(const std::string& text, bool submit, EditorPreferences& preferences) {
    auto problem = OpenAgent(preferences);
    if (_agentSetup.Active()) {
        _pendingAgentText += text;
        _pendingAgentSubmit = _pendingAgentSubmit || submit;
    } else if (const auto agent = _console.AgentSession()) {
        _console.SendTo(*agent, text, submit);
    }
    return problem;
}
void EditorWorkspace::HandleAgent(EditorAgentRequest request, const EditorSnapshot& snapshot,
                                  EditorPreferences& preferences, EditorWorkspaceResult& result) {
    const auto failure = [&] {
        return BuildFailureContext(snapshot.diagnostics.Attempt(), snapshot.diagnostics.AttemptEntries(),
                                   AgentContextMaxLines);
    };
    switch (request) {
    case EditorAgentRequest::Open: result.agentProblem = OpenAgent(preferences); break;
    case EditorAgentRequest::NewShell:
        ShowConsole(preferences);
        _console.OpenShell();
        break;
    case EditorAgentRequest::SendScene: result.sendScene = true; break;
    case EditorAgentRequest::SendBuildFailure:
        if (const auto text = failure(); !text.empty()) result.agentProblem = SendToAgent(text, false, preferences);
        break;
    case EditorAgentRequest::FixBuild:
        if (const auto text = failure(); !text.empty())
            result.agentProblem = SendToAgent(text + AgentFixBuildRequest, true, preferences);
        break;
    }
    result.cancelViewportInput = true;
}
EditorAgentStatus EditorWorkspace::AgentStatus() const {
    EditorAgentStatus status{.control = _agentControl};
    if (const auto agent = _console.AgentSession()) {
        const auto session = _console.Sessions()[*agent];
        status.session = true;
        status.title = session.title;
        status.state = session.state;
        status.activity = session.activity;
    }
    return status;
}
void EditorWorkspace::ToggleBottom(EditorPreferences& preferences, std::optional<EditorPanel> panel) {
    const auto target = panel.value_or(preferences.lastBottomPanel);
    auto& visible = preferences.Visible(target);
    visible = !visible;
    if (visible) {
        preferences.lastBottomPanel = target;
        _focusBottom = target;
    }
}
bool EditorWorkspace::Shortcuts(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                EditorShellResult& result) const {
    constexpr auto route = ImGuiInputFlags_RouteGlobal;
    // Register stable global routes even when a popup/text field temporarily blocks execution.
    const bool palette = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P, route);
    const bool catalog = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_P, route);
    const bool bottom = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_J, route);
    const bool console = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_GraveAccent, route);
    const bool agent = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_A, route);
    const bool primary = ImGui::Shortcut(ImGuiKey_F5, route);
    const auto& io = ImGui::GetIO();
    const bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    // The Console owns ordinary chords; global panel, palette and agent shortcuts remain available.
    const bool blocked = popup || io.WantTextInput || _consoleFocused;
    bool consumed = false;
    if (palette && !popup) {
        result.openCommands = true;
        consumed = true;
    }
    if (console && !popup) {
        result.bottomPanel = EditorPanel::Console;
        consumed = true;
    }
    if (agent && !popup) {
        result.agent = EditorAgentRequest::Open;
        consumed = true;
    }
    if (catalog && !popup && !_consoleFocused) {
        result.focusCatalog = true;
        consumed = true;
    }
    if (bottom && !popup && !io.WantTextInput) {
        result.toggleBottom = true;
        consumed = true;
    }
    if (primary && !blocked) {
        const auto command = EditorPrimaryCommand(snapshot);
        if (snapshot.capabilities.Can(command)) actions.command(command);
        consumed = true;
    }
    for (const auto& command : EditorCommands()) {
        if (!command.shortcut) continue;
        if (ImGui::Shortcut(command.shortcut, route) && !blocked) {
            if (snapshot.capabilities.Can(command.command)) actions.command(command.command);
            consumed = true;
        }
    }
    return consumed;
}
void EditorWorkspace::DockHost(ImVec2 position, ImVec2 size, bool reset, const EditorPreferences& preferences) {
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("Workspace###editor.workspace", nullptr, HostFlags);
    const ImGuiID dock = ImGui::GetID("editor.dockspace");
    if (reset || !ImGui::DockBuilderGetNode(dock)) {
        ImGui::DockBuilderRemoveNode(dock);
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, size);
        ImGuiID center = dock;
        std::array<ImGuiID, 4> groups{};
        const bool agentLayout = preferences.layout == EditorWorkspaceLayout::Agent;
        if (agentLayout) // The Console (agent session) and Inspector share a tall right column
            groups[static_cast<std::size_t>(EditorDockGroup::Right)] =
                ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, AgentColumnRatio, nullptr, &center);
        if (BottomVisible(preferences) && !agentLayout)
            groups[static_cast<std::size_t>(EditorDockGroup::Bottom)] =
                ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.30f, nullptr, &center);
        const bool catalog = preferences.visible[static_cast<std::size_t>(EditorPanel::Catalog)];
        if (catalog)
            groups[static_cast<std::size_t>(EditorDockGroup::Left)] =
                ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
        if (!agentLayout && (preferences.visible[static_cast<std::size_t>(EditorPanel::Inspector)] ||
                             preferences.visible[static_cast<std::size_t>(EditorPanel::Tuning)])) {
            const float ratio = preferences.layout == EditorWorkspaceLayout::Inspect ? 0.32f : 0.24f;
            groups[static_cast<std::size_t>(EditorDockGroup::Right)] =
                ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, ratio / (catalog ? 0.80f : 1.0f), nullptr, &center);
        }
        _centerDock = center;
        groups[static_cast<std::size_t>(EditorDockGroup::Center)] = center;
        auto& bottom = groups[static_cast<std::size_t>(EditorDockGroup::Bottom)];
        // Audio Preview stays in sight beside the Assets it plays from.
        const ImGuiID audio =
            bottom && preferences.visible[static_cast<std::size_t>(EditorPanel::AudioPreview)]
                ? ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Right, AudioColumnRatio, nullptr, &bottom)
                : 0;
        for (const auto& panel : EditorPanels) {
            const auto index = static_cast<std::size_t>(panel.id);
            _dockOnOpen[index] = !preferences.visible[index];
            const auto group =
                agentLayout && panel.dock == EditorDockGroup::Bottom ? EditorDockGroup::Right : panel.dock;
            if (!preferences.visible[index]) continue;
            ImGui::DockBuilderDockWindow(panel.window, panel.id == EditorPanel::AudioPreview && audio
                                                           ? audio
                                                           : groups[static_cast<std::size_t>(group)]);
        }
        if (agentLayout) _focusBottom = EditorPanel::Console; // In front of the Inspector
        ImGui::DockBuilderFinish(dock);
        _ignoreLayoutChanges = 3;
    }
    if (auto* central = ImGui::DockBuilderGetCentralNode(dock)) _centerDock = central->ID;
    if (!ImGui::DockBuilderGetNode(_centerDock)) _centerDock = dock;
    // A panel hidden by a preset gets its default dock only when first opened. Existing user docks/floats survive.
    for (const auto& panel : EditorPanels) {
        const auto index = static_cast<std::size_t>(panel.id);
        if (!preferences.visible[index] || !_dockOnOpen[index]) continue;
        _dockOnOpen[index] = false;
        const auto saved = SavedPanelDock(panel);
        if (saved && ImGui::DockBuilderGetNode(saved)) continue;
        ImGuiID target = 0;
        const auto assetsDock = SavedPanelDock(EditorPanelInfo(EditorPanel::Assets));
        if (panel.id == EditorPanel::AudioPreview && assetsDock && ImGui::DockBuilderGetNode(assetsDock)) {
            ImGuiID rest = 0; // Audio Preview stays in sight beside the Assets it plays from
            target = ImGui::DockBuilderSplitNode(assetsDock, ImGuiDir_Right, AudioColumnRatio, nullptr, &rest);
        } else {
            for (const auto& peer : EditorPanels)
                if (peer.id != panel.id && peer.id != EditorPanel::AudioPreview && peer.dock == panel.dock) {
                    const auto peerDock = SavedPanelDock(peer);
                    if (peerDock && ImGui::DockBuilderGetNode(peerDock)) target = peerDock;
                }
        }
        if (!target) {
            switch (panel.dock) {
            case EditorDockGroup::Left:
                target = ImGui::DockBuilderSplitNode(_centerDock, ImGuiDir_Left, 0.20f, nullptr, &_centerDock);
                break;
            case EditorDockGroup::Right:
                target = ImGui::DockBuilderSplitNode(_centerDock, ImGuiDir_Right, 0.30f, nullptr, &_centerDock);
                break;
            case EditorDockGroup::Bottom:
                target = ImGui::DockBuilderSplitNode(_centerDock, ImGuiDir_Down, 0.30f, nullptr, &_centerDock);
                break;
            case EditorDockGroup::Center: target = _centerDock; break;
            }
        }
        ImGui::DockBuilderDockWindow(panel.window, target);
        ImGui::DockBuilderFinish(dock);
    }
    ImGui::DockSpace(dock, {}, _maximized ? ImGuiDockNodeFlags_KeepAliveOnly : ImGuiDockNodeFlags_None);
    ImGui::End();
    ImGui::PopStyleVar(2);
}
EditorWorkspaceResult EditorWorkspace::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                            const ViewportImage& image, EditorPreferences& preferences, int& fontSize) {
    const bool opening = !_initialized;
    if (opening) {
        // View her oturumun başında açıktır; kayıtlı kapalı / daraltılmış hali başlangıcı gizlemez.
        preferences.Visible(EditorPanel::Viewport) = true;
        if (!preferences.docking.empty())
            ImGui::LoadIniSettingsFromMemory(preferences.docking.data(), preferences.docking.size());
        for (const auto& panel : EditorPanels)
            _dockOnOpen[static_cast<std::size_t>(panel.id)] =
                (preferences.layout != EditorWorkspaceLayout::Custom &&
                 !preferences.visible[static_cast<std::size_t>(panel.id)]) ||
                !ImGui::FindWindowSettingsByID(ImHashStr(panel.window));
        _initialized = true;
        _ignoreLayoutChanges = 3;
    }
    if (_pendingToggle) {
        SetMaximized(!_maximized, preferences);
        _pendingToggle = false;
    }
    _console.Update();
    const auto oldVisibility = preferences.visible;
    EditorWorkspaceResult result;
    if (_showFailedOutput) {
        SetMaximized(false, preferences);
        preferences.Visible(EditorPanel::Output) = true;
        preferences.lastBottomPanel = EditorPanel::Output;
        _focusBottom = EditorPanel::Output;
        _output.ShowLastAttempt(snapshot.diagnostics.Attempt().id);
        _viewport.ReleaseFocus();
        _showFailedOutput = false;
        result.cancelViewportInput = true;
    }
    const auto& attempt = snapshot.diagnostics.Attempt();
    const bool failed =
        attempt.result == EditorBuildResult::BuildFailed || attempt.result == EditorBuildResult::RefreshFailed;
    constexpr const char* FailurePopup = "Build & Refresh failed";
    if (failed && attempt.id != _failureAttempt && attempt.id != snapshot.controlAttempt &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        _failureAttempt = attempt.id;
        std::string detail = attempt.result == EditorBuildResult::RefreshFailed ? "Build succeeded, refresh failed."
                                                                                : "Build & Refresh could not complete.";
        if (attempt.exitCode) detail += "\nBuild exit code: " + std::to_string(*attempt.exitCode);
        if (attempt.firstError) detail += "\n\n" + attempt.firstError->text;
        _failureDetail.assign(detail.begin(), detail.end());
        _failureDetail.push_back(0);
        ImGui::OpenPopup(FailurePopup);
        _viewport.ReleaseFocus();
        result.cancelViewportInput = true;
    }
    auto menu = _shell.Menu(snapshot, actions, preferences, fontSize, _maximized, _offerProjectList);
    result.cancelViewportInput |= Shortcuts(snapshot, actions, menu);
    if (_palette.Draw(snapshot, actions, preferences, _maximized, menu)) {
        result.cancelViewportInput = true;
        _viewport.ReleaseFocus();
    }
    const auto* viewport = ImGui::GetMainViewport();
    const float menuHeight = ImGui::GetFrameHeight();
    const float toolbarHeight =
        _shell.Toolbar(snapshot, actions, {viewport->Pos.x, viewport->Pos.y + menuHeight}, viewport->Size.x, _marking);
    const float statusHeight =
        _shell.Status(snapshot, preferences, AgentStatus(), {viewport->Pos.x, viewport->Pos.y + viewport->Size.y},
                      viewport->Size.x, menu);
    if (_fixFailedBuild) {
        menu.agent = EditorAgentRequest::FixBuild;
        _fixFailedBuild = false;
    }
    if (menu.agent) HandleAgent(*menu.agent, snapshot, preferences, result);
    if (menu.toggleMaximize) SetMaximized(!_maximized, preferences);
    if (menu.toggleBottom || menu.bottomPanel) ToggleBottom(preferences, menu.bottomPanel);
    if (menu.focusCatalog) {
        SetMaximized(false, preferences);
        preferences.Visible(EditorPanel::Catalog) = true;
        _catalog.FocusSearch();
        _focusCatalog = true;
    }
    if (menu.layout) ApplyLayout(*menu.layout, preferences);
    if (_maximized && oldVisibility != preferences.visible) SetMaximized(false, preferences);
    for (const auto panel : EditorBottomPanels)
        if (!oldVisibility[static_cast<std::size_t>(panel)] && preferences.Visible(panel)) {
            _focusBottom = panel;
            preferences.lastBottomPanel = panel;
        }
    if (menu.focusCatalog || menu.toggleBottom || menu.bottomPanel || menu.layout || menu.toggleMaximize)
        result.cancelViewportInput = true;
    const ImVec2 workspacePosition{viewport->Pos.x, viewport->Pos.y + menuHeight + toolbarHeight};
    const ImVec2 workspaceSize{viewport->Size.x,
                               std::max(1.0f, viewport->Size.y - menuHeight - toolbarHeight - statusHeight)};
    const ImVec2 headerPadding{ImGui::GetStyle().FramePadding.x, ToolMetrics::PanelHeaderPadding * ToolUiScale()};
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, PanelMinimumSize(workspaceSize));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, headerPadding);
    DockHost(workspacePosition, workspaceSize, _resetDock, preferences);
    ImGui::PopStyleVar();
    _resetDock = false;
    result.quit = menu.quit;
    result.returnToProjectList = menu.returnToProjectList;
    if (_maximized) {
        ImGui::SetNextWindowPos(workspacePosition);
        ImGui::SetNextWindowSize(workspaceSize);
        constexpr auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking |
                               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;
        if (ImGui::Begin("Viewport###editor.maximized", nullptr, flags))
            result.interaction = _viewport.Draw(snapshot, actions, image, preferences.showMetrics, true);
        ImGui::End();
    } else {
        constexpr auto flags = ImGuiWindowFlags_NoCollapse;
        bool viewportDrawn = false;
        bool consoleDrawn = false;
        bool focusedBottomDrawn = false;
        for (const auto& panel : EditorPanels) {
            if (!preferences.Visible(panel.id)) continue;
            const bool focusViewport = _focusViewport && panel.id == EditorPanel::Viewport;
            if (focusViewport || (_focusBottom && *_focusBottom == panel.id) ||
                (_focusCatalog && panel.id == EditorPanel::Catalog))
                ImGui::SetNextWindowFocus();
            if (focusViewport) ImGui::SetNextWindowCollapsed(false);
            const bool edgeToEdge = panel.id == EditorPanel::Console || panel.id == EditorPanel::Assets ||
                                    panel.id == EditorPanel::AudioPreview;
            if (edgeToEdge) ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, headerPadding);
            const bool drawPanel =
                ImGui::Begin(panel.window, &preferences.Visible(panel.id),
                             flags | (panel.id == EditorPanel::Viewport || panel.id == EditorPanel::Inspector ||
                                              panel.id == EditorPanel::Assets || panel.id == EditorPanel::AudioPreview
                                          ? ImGuiWindowFlags_NoScrollbar
                                          : 0));
            ImGui::PopStyleVar(); // Panel contents keep their regular control spacing.
            if (edgeToEdge) ImGui::PopStyleVar();
            if (drawPanel) {
                if (_focusBottom && *_focusBottom == panel.id) focusedBottomDrawn = true;
                if (panel.dock == EditorDockGroup::Bottom && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
                    preferences.lastBottomPanel = panel.id;
                switch (panel.id) {
                case EditorPanel::Catalog: _catalog.Draw(snapshot, actions, preferences); break;
                case EditorPanel::Viewport:
                    result.interaction = _viewport.Draw(snapshot, actions, image, preferences.showMetrics, false);
                    viewportDrawn = preferences.Visible(panel.id);
                    break;
                case EditorPanel::Inspector:
                    _inspector.Draw(snapshot, actions);
                    if (const auto text = _inspector.TakeAgentText(); !text.empty())
                        result.agentProblem = SendToAgent(text, false, preferences);
                    break;
                case EditorPanel::Output:
                    _output.Draw(snapshot, actions, preferences);
                    if (const auto text = _output.TakeAgentText(); !text.empty())
                        result.agentProblem = SendToAgent(text, false, preferences);
                    break;
                case EditorPanel::Profiler: _profiler.Draw(snapshot, actions); break;
                case EditorPanel::Tuning  : _tuning.Draw(snapshot, actions); break;
                case EditorPanel::Console:
                    if (const auto request = _console.Draw()) result.agentProblem = AddSession(*request, preferences);
                    consoleDrawn = true;
                    break;
                case EditorPanel::Assets      : _assets.Draw(snapshot, actions); break;
                case EditorPanel::AudioPreview: _audioPreview.Draw(snapshot, actions); break;
                case EditorPanel::Count       : break;
                }
            }
            ImGui::End();
        }
        if (_focusBottom) {
            SelectPanelTab(EditorPanelInfo(*_focusBottom).window);
            if (focusedBottomDrawn) _focusBottom.reset();
        }
        _focusCatalog = false;
        _consoleFocused = consoleDrawn && _console.Focused();
        if (!viewportDrawn) {
            _viewport.ReleaseFocus();
            result.interaction = {};
        }
        if (_focusViewport) {
            SelectPanelTab(EditorPanelInfo(EditorPanel::Viewport).window);
            ImGui::SetWindowFocus(EditorPanelInfo(EditorPanel::Viewport).window);
            // Dock sekmesi ilk karede henüz hazır olmayabilir; View gerçekten çizilene kadar seçilir.
            if (viewportDrawn) _focusViewport = false;
        }
    }
    ImGui::PopStyleVar();
    if (preferences.visible != oldVisibility && !menu.layout) {
        preferences.layout = EditorWorkspaceLayout::Custom;
        result.cancelViewportInput = true;
    }
    if (result.interaction.toggleMaximize) {
        _pendingToggle = true;
        result.cancelViewportInput = true;
    }
    if (_ignoreLayoutChanges > 0) {
        // Clear ImGui's delayed dirty timer after programmatic docking has settled.
        CaptureLayout(preferences);
        --_ignoreLayoutChanges;
    } else if (!_maximized && ImGui::GetIO().WantSaveIniSettings) {
        CaptureLayout(preferences);
    }
    ImGui::SetNextWindowSize({ImGui::GetFontSize() * 40, 0}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(FailurePopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (attempt.id != _failureAttempt) ImGui::CloseCurrentPopup(); // A newer Build & Refresh replaced it
        ImGui::TextWrapped("Select text with the mouse, then Ctrl+C. Ctrl+A selects all text.");
        ImGui::InputTextMultiline("##failure-detail", _failureDetail.data(), _failureDetail.size(),
                                  {-1, ImGui::GetTextLineHeightWithSpacing() * 8}, ImGuiInputTextFlags_ReadOnly);
        ImGui::Separator();
        if (ToolButton("Ask Agent to Fix", {}, true)) {
            _fixFailedBuild = true; // Handled next frame, outside this popup
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ToolButton("Copy")) ImGui::SetClipboardText(_failureDetail.data());
        ImGui::SameLine();
        if (ToolButton("Show Output")) {
            _showFailedOutput = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ToolButton("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        result.cancelViewportInput = true;
    }
    if (_agentSetup.Active()) {
        result.cancelViewportInput = true;
        const auto setup = _agentSetup.Draw(preferences.agent);
        if (setup == EditorAgentSetupResult::Started) {
            const auto text = std::exchange(_pendingAgentText, {});
            const bool submit = std::exchange(_pendingAgentSubmit, false);
            result.agentProblem = text.empty() ? OpenAgent(preferences) : SendToAgent(text, submit, preferences);
        } else if (setup == EditorAgentSetupResult::Canceled) {
            _pendingAgentText.clear();
            _pendingAgentSubmit = false;
        }
    }
    if (_marking) MarkOverlay(snapshot, actions, menuHeight + toolbarHeight, result);
    if (result.cancelViewportInput) _viewport.ReleaseFocus();
    return result;
}
