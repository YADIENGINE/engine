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
constexpr float GitDrawerWidth = 340.0f;
constexpr float GitDrawerMaximumShare = 0.5f; // Of the workspace, in a narrow window
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
void DrawMarkHint(const std::string& text, ImVec2 origin, ImVec2 area) {
    const float pad = 8.0f * ToolUiScale();
    const float wrap = std::max(1.0f, std::min(700.0f * ToolUiScale(), area.x - pad * 4));
    const auto size = ImGui::CalcTextSize(text.c_str(), nullptr, false, wrap);
    const ImVec2 at{origin.x + (area.x - size.x) * 0.5f, origin.y + pad};
    auto* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled({at.x - pad, at.y - pad * 0.5f}, {at.x + size.x + pad, at.y + size.y + pad * 0.5f},
                        ImGui::GetColorU32(ImGuiCol_PopupBg), ImGui::GetStyle().FrameRounding);
    draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), at, ImGui::GetColorU32(ImGuiCol_Text), text.c_str(), nullptr,
                  wrap);
}
} // namespace
void EditorWorkspace::NotifyContext(std::string text, bool failed) {
    _contextNotice = std::move(text);
    _contextNoticeFailed = failed;
    _contextNoticeAt = std::chrono::steady_clock::now();
}
void EditorWorkspace::ContextNotice(ImVec2 position, ImVec2 size) const {
    const double age = std::chrono::duration<double>(std::chrono::steady_clock::now() - _contextNoticeAt).count();
    const double duration = _contextNoticeFailed ? 5.0 : 3.0;
    if (_contextNotice.empty() || age >= duration) return;
    const float pad = 12.0f * ToolUiScale();
    const float width = std::max(1.0f, std::min(420.0f * ToolUiScale(), size.x - pad * 2));
    ImGui::SetNextWindowPos({position.x + size.x - pad, position.y + size.y - pad}, ImGuiCond_Always, {1, 1});
    ImGui::SetNextWindowSize({width, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, static_cast<float>(std::clamp((duration - age) / 0.75, 0.0, 1.0)));
    constexpr auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                           ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin("##editor-context-notice", nullptr, flags)) {
        if (_contextNoticeFailed) ImGui::PushStyleColor(ImGuiCol_Text, EditorLevelColor(EditorLogLevel::Warning));
        ImGui::TextWrapped("%s", _contextNotice.c_str());
        if (_contextNoticeFailed) ImGui::PopStyleColor();
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    }
    ImGui::End();
    ImGui::PopStyleVar();
}
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
            preferences.Visible(EditorPanel::Profiler) = true;
            preferences.lastBottomPanel = EditorPanel::Profiler;
        }
        if (layout == EditorWorkspaceLayout::Agent) {
            preferences.Visible(EditorPanel::Catalog) = false;
            preferences.Visible(EditorPanel::Console) = true;
            preferences.lastBottomPanel = EditorPanel::Console;
        }
        _resetDock = true;
    }
    _ignoreLayoutChanges = 3;
    _viewport.ReleaseFocus();
}
void EditorWorkspace::MarkOverlay(const EditorSnapshot& snapshot, float top, EditorWorkspaceResult& result) {
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
        const auto target = PasteContextTarget(_sessions.Infos());
        const std::string scope = !_overlayStart ? "Scene image or editor window"
                                  : onViewport(*_overlayStart) && onViewport(io.MousePos) ? "Scene region"
                                                                                          : "Editor window region";
        auto hint = scope + " \xC2\xB7 Paste mark to " + target.title +
                    " \xC2\xB7 You submit\n"
                    "Drag or click. Both ends on the scene image: scene capture. Otherwise: editor window.\n"
                    "Starting on the scene pauses it. Esc / right-click cancels.";
        if (!target.unavailable.empty()) hint += "\n" + target.unavailable;
        DrawMarkHint(hint, origin, size);
        const bool cancel =
            ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        if (cancel) {
            _overlayStart.reset();
            _marking = false;
        }
        if (!cancel && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            _overlayStart = io.MousePos;
            // On the viewport the frame holds still, as with Alt+drag
            if (onViewport(io.MousePos) && !snapshot.paused && snapshot.capabilities.Can(EditorCommand::Pause))
                result.interaction.pauseForMarking = true;
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
    }
    ImGui::End();
    ImGui::PopStyleVar();
    result.cancelViewportInput = true;
}
void EditorWorkspace::OpenNote(const std::filesystem::path& path, EditorPreferences& preferences) {
    if (_maximized) SetMaximized(false, preferences);
    (void)_notes.Open(path);
    preferences.Visible(EditorPanel::Notes) = true;
    _focusViewport = false;
    _focusNotes = true;
    _viewport.ReleaseFocus();
}
void EditorWorkspace::ShowConsole(EditorPreferences& preferences) {
    if (_maximized) SetMaximized(false, preferences);
    preferences.Visible(EditorPanel::Console) = true;
    preferences.lastBottomPanel = EditorPanel::Console;
    _focusBottom = EditorPanel::Console;
    _viewport.ReleaseFocus();
}
void EditorWorkspace::ShowAgents(std::optional<std::uint64_t> session, EditorPreferences& preferences) {
    (void)preferences;
    _agentsDrawer.Open(session ? session : AgentStatusFocus());
    _viewport.ReleaseFocus();
}
void EditorWorkspace::PasteToConsole(const std::string& text, EditorPreferences& preferences,
                                     const std::string& context) {
    if (const auto agent = _sessions.AgentSession()) {
        _sessions.Select(*agent);
        ShowAgents(agent, preferences);
    } else {
        ShowConsole(preferences);
    }
    _sessions.Paste(text, context);
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
    // The agent session requests go to is reused when it runs the chosen CLI; otherwise a new session starts.
    // Either way the Agents drawer shows it: agents are worked with there.
    const auto commandLine = AgentCommandLine(DefaultTerminalShell(), command);
    if (const auto agent = _sessions.AgentSession(); agent && _sessions.Info(*agent)->agent == kind.id) {
        _sessions.Revive(*agent, commandLine);
        ShowAgents(agent, preferences);
    } else {
        ShowAgents(_sessions.AddAgent(kind.id, commandLine), preferences);
    }
    return {};
}
std::string EditorWorkspace::AddSession(const ConsoleNewSession& request, EditorPreferences& preferences,
                                        bool inDrawer) {
    if (!request.agent) {
        ShowConsole(preferences);
        _sessions.OpenShell();
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
    const auto id = _sessions.AddAgent(*request.agent, AgentCommandLine(DefaultTerminalShell(), command));
    if (inDrawer)
        ShowAgents(id, preferences);
    else
        ShowConsole(preferences);
    return {};
}
std::string EditorWorkspace::SendToAgent(const std::string& text, bool submit, EditorPreferences& preferences,
                                         const std::string& context) {
    auto problem = OpenAgent(preferences);
    if (_agentSetup.Active()) {
        _pendingAgentText += text;
        _pendingAgentSubmit = _pendingAgentSubmit || submit;
        if (!context.empty()) {
            if (!_pendingAgentContext.empty()) _pendingAgentContext += " + ";
            _pendingAgentContext += context;
            NotifyContext(context + " waiting for agent setup \xC2\xB7 Not pasted");
        }
    } else if (const auto agent = _sessions.AgentSession()) {
        _sessions.SendTo(*agent, text, submit, context);
        if (!context.empty())
            NotifyContext(context + " queued for " + _sessions.Info(*agent)->title + " \xC2\xB7 Waiting to paste");
    } else if (!context.empty()) {
        NotifyContext("Could not paste " + context + ": no agent terminal is available.", true);
    }
    return problem;
}
std::string EditorWorkspace::SendToSession(std::uint64_t id, const std::string& text, bool submit,
                                           const std::string& context) {
    const auto target = SessionContextTarget(_sessions.Infos(), id);
    if (!target.unavailable.empty()) {
        if (!context.empty()) NotifyContext(context + " could not be pasted. " + target.unavailable, true);
        return target.unavailable;
    }
    _sessions.SendTo(id, text, submit, context);
    if (!context.empty()) NotifyContext(context + " queued for " + target.title + " \xC2\xB7 Waiting to paste");
    return {};
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
        _sessions.OpenShell();
        break;
    case EditorAgentRequest::SendScene: result.sendScene = true; break;
    case EditorAgentRequest::SendBuildFailure:
        if (const auto text = failure(); !text.empty())
            result.agentProblem = SendToAgent(text, false, preferences, "Build failure");
        break;
    case EditorAgentRequest::FixBuild:
        if (const auto text = failure(); !text.empty())
            result.agentProblem = SendToAgent(text + AgentFixBuildRequest, true, preferences, "Fix request");
        break;
    }
    result.cancelViewportInput = true;
}
std::optional<std::uint64_t> EditorWorkspace::AgentStatusFocus() const {
    // A session that asks the developer something comes first; then the one agent requests go to.
    for (const auto& info : _sessions.Infos())
        if (info.agent && info.state == SessionState::Waiting) return info.id;
    return _sessions.AgentSession();
}
EditorAgentStatus EditorWorkspace::AgentStatus(const EditorSnapshot& snapshot) const {
    EditorAgentStatus status{.control = !snapshot.agents || snapshot.agents->control};
    status.drawerOpen = _agentsDrawer.IsOpen();
    const auto* hold = snapshot.agents && snapshot.agents->hold ? &*snapshot.agents->hold : nullptr;
    for (const auto& info : _sessions.Infos())
        if (info.agent) status.sessions.push_back({info.state, hold && hold->session == std::to_string(info.id)});
    if (const auto agent = AgentStatusFocus()) {
        const auto session = *_sessions.Info(*agent);
        status.session = true;
        status.focus = session.id;
        status.sessionId = std::to_string(session.id);
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
bool EditorWorkspace::Shortcuts(std::span<const EditorAction> list) const {
    const auto* notes = ImGui::FindWindowByName(EditorPanelInfo(EditorPanel::Notes).window);
    const auto* focused = ImGui::GetCurrentContext()->NavWindow;
    const bool inNotes = _notes.Focused() || (notes && focused && focused->RootWindow == notes);
    const bool ownsKeys = _consoleFocused || inNotes; // Both keep their keystrokes from the narrower shortcuts
    constexpr auto route = ImGuiInputFlags_RouteGlobal;
    const bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    const bool typing = ImGui::GetIO().WantTextInput;
    const bool drawer = _agentsDrawer.IsOpen();
    bool consumed = false;
    for (const auto& binding : EditorKeyBindings()) {
        // Routes are registered every frame, even when a popup or text field holds the key back this time.
        if (binding.scope == EditorKeyScope::AgentsDrawer && !drawer) continue;
        if (!ImGui::Shortcut(binding.chord, route) || popup) continue;
        if (binding.scope == EditorKeyScope::OutsideConsole && ownsKeys) continue;
        if (binding.scope == EditorKeyScope::Playback && (typing || ownsKeys)) continue;
        consumed = true;
        const auto action = std::ranges::find(list, binding.key, &EditorAction::key);
        if (action != list.end() && action->enabled && action->run) action->run();
    }
    return consumed;
}
void EditorWorkspace::DockHost(ImVec2 position, ImVec2 size, bool reset, const EditorPreferences& preferences) {
    ImGui::SetNextWindowPos(position);
    ImGui::SetNextWindowSize(size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToolColors().window);
    ImGui::Begin("Workspace###editor.workspace", nullptr, HostFlags);
    ImGui::PopStyleColor();
    const ImGuiID dock = ImGui::GetID("editor.dockspace");
    if (reset || !ImGui::DockBuilderGetNode(dock)) {
        ImGui::DockBuilderRemoveNode(dock);
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, size);
        ImGuiID center = dock;
        std::array<ImGuiID, 4> groups{};
        const bool agentLayout = preferences.layout == EditorWorkspaceLayout::Agent;
        if (agentLayout) { // The Console (agent session) and Inspector share a tall right column
            groups[static_cast<std::size_t>(EditorDockGroup::Right)] =
                ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, AgentColumnRatio, nullptr, &center);
        }
        if (BottomVisible(preferences) && !agentLayout)
            groups[static_cast<std::size_t>(EditorDockGroup::Bottom)] =
                ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.30f, nullptr, &center);
        const bool catalog = preferences.visible[static_cast<std::size_t>(EditorPanel::Catalog)];
        if (catalog)
            groups[static_cast<std::size_t>(EditorDockGroup::Left)] =
                ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
        if (!agentLayout && preferences.visible[static_cast<std::size_t>(EditorPanel::Inspector)]) {
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
    // Panes are told apart by their gaps: the splitters take the frame's color until touched. A tab closes itself on
    // hover, so the tab bar's own close button would only repeat it.
    ImGui::PushStyleColor(ImGuiCol_Separator, ToolColors().window);
    ImGuiDockNodeFlags dockFlags = ImGuiDockNodeFlags_NoCloseButton;
    if (_maximized) dockFlags |= ImGuiDockNodeFlags_KeepAliveOnly;
    ImGui::DockSpace(dock, {}, dockFlags);
    ImGui::PopStyleColor();
    ImGui::End();
    ImGui::PopStyleVar(2);
}
void EditorWorkspace::GitOverlay(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                 const EditorShellResult& menu, ImVec2 position, ImVec2 size,
                                 EditorPreferences& preferences, EditorWorkspaceResult& result) {
    if (!snapshot.git) return;
    const auto& git = *snapshot.git;
    if (menu.git.toggle) {
        _gitDrawer.Toggle(*menu.git.toggle);
        if (_gitDrawer.IsOpen()) result.cancelViewportInput = true;
    }
    if (menu.git.show) {
        _gitDrawer.Show(*menu.git.show);
        result.cancelViewportInput = true;
    }
    const auto& io = ImGui::GetIO();
    const bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    // Escape backs out one step, unless it is the scene's or the Console's key right now.
    if (_gitDrawer.IsOpen() && !_agentsDrawer.IsOpen() && !popup && !io.WantTextInput && !_consoleFocused &&
        !_viewportFocused && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        if (git.diff && actions.git)
            actions.git(GitRequest{.kind = GitRequestKind::CloseDiff});
        else
            _gitDrawer.Close();
    }
    if (!_gitDrawer.IsOpen()) {
        if (git.diff && actions.git) actions.git(GitRequest{.kind = GitRequestKind::CloseDiff});
        return;
    }
    const float width = std::min(GitDrawerWidth * ToolUiScale(), size.x * GitDrawerMaximumShare);
    _gitDrawer.Draw(git, actions, position, {width, size.y}, snapshot.building);
    if (const auto text = _gitDrawer.TakeAgentText(); !text.empty())
        result.agentProblem = SendToAgent(text, true, preferences);
    const auto bringForward = [&](const char* name) {
        // Above whichever docked panel was clicked last, and below any popup or tooltip: asked after drawing, so a
        // menu the drawer has just opened is not pushed behind it.
        if (_marking || _agentSetup.Active() ||
            ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
            return;
        if (auto* window = ImGui::FindWindowByName(name)) ImGui::BringWindowToDisplayFront(window);
    };
    bringForward("Source control###editor.git-drawer");
    if (git.diff) {
        // The scene's place when it has one on screen; otherwise everything right of the drawer.
        ImVec2 from{position.x + width, position.y};
        ImVec2 room{size.x - width, size.y};
        if (const auto* scene = ImGui::FindWindowByName(EditorPanelInfo(EditorPanel::Viewport).window);
            scene && !_maximized && preferences.Visible(EditorPanel::Viewport) && scene->Active) {
            const float left = std::max(scene->Pos.x, from.x);
            const float right = scene->Pos.x + scene->Size.x;
            if (right - left > width) {
                from = {left, scene->Pos.y};
                room = {right - left, scene->Size.y};
            }
        }
        _gitDiff.Draw(*git.diff, actions, from, room);
        bringForward("Change###editor.git-diff");
        result.cancelViewportInput = true;
    }
}

void EditorWorkspace::AgentsOverlay(const EditorSnapshot& snapshot, const EditorUiActions& actions, ImVec2 position,
                                    ImVec2 size, EditorPreferences& preferences, EditorWorkspaceResult& result) {
    if (!_agentsDrawer.IsOpen()) return;
    const bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    // Escape closes it, unless the agent's terminal, the scene or a text field has the keyboard (an agent CLI uses
    // Escape to interrupt).
    if (!popup && !ImGui::GetIO().WantTextInput && !_agentsDrawer.TerminalFocused() && !_viewportFocused &&
        ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        _agentsDrawer.Close();
        return;
    }
    if (_gitDrawer.IsOpen() && snapshot.git) { // Both open: side by side, Git on the left
        const float git = std::min(GitDrawerWidth * ToolUiScale(), size.x * GitDrawerMaximumShare);
        position.x += git;
        size.x -= git;
    }
    const auto infos = _sessions.Infos();
    _agentsDrawer.SetPreferred(preferences.agent.kind);
    const auto request = _agentsDrawer.Draw(
        infos, [this](std::uint64_t id) { return _sessions.Terminal(id); }, _views, snapshot, actions, position, size);
    // Above whichever docked panel was clicked last; never over a popup, and checked after drawing, since a menu the
    // drawer opened this frame would otherwise stay behind it
    const bool popupNow = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    if (!popupNow && !_marking && !_agentSetup.Active())
        if (auto* window = ImGui::FindWindowByName("Agents###editor.agents-drawer"))
            ImGui::BringWindowToDisplayFront(window);
    if (request.openConsole) {
        _sessions.Select(*request.openConsole);
        _agentsDrawer.Close();
        ShowConsole(preferences);
    }
    if (request.sendScene) {
        result.sendScene = true;
        result.sendSceneSession = request.sendScene;
    }
    if (request.close) _sessions.Close(*request.close);
    if (request.restart) _sessions.Revive(*request.restart, {});
    if (request.add) result.agentProblem = AddSession(ConsoleNewSession{*request.add}, preferences, true);
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
    _sessions.Update(snapshot.agents);
    const auto oldVisibility = preferences.visible;
    EditorWorkspaceResult result;
    for (const auto& delivery : _sessions.TakeContextDeliveries()) {
        if (!delivery.problem.empty()) {
            const auto problem =
                delivery.context + " could not be pasted to " + delivery.target + ". " + delivery.problem;
            NotifyContext(problem, true);
            result.agentProblem = problem;
        } else {
            NotifyContext(delivery.context + (delivery.submitted ? " submitted to " : " pasted to ") + delivery.target +
                          (delivery.submitted ? "" : " \xC2\xB7 Not submitted"));
        }
    }
    const auto sessionInfos = _sessions.Infos();
    const auto agentTarget = AgentContextTarget(sessionInfos, preferences.agent.kind);
    const auto sceneTarget =
        _agentsDrawer.IsOpen() ? SessionContextTarget(sessionInfos, _agentsDrawer.ChosenSession()) : agentTarget;
    const auto markTarget = PasteContextTarget(sessionInfos);
    if (_showFailedOutput) {
        SetMaximized(false, preferences);
        preferences.Visible(EditorPanel::Output) = true;
        preferences.lastBottomPanel = EditorPanel::Output;
        _focusBottom = EditorPanel::Output;
        // A failure without compiler messages (a worker that would not start) is only readable in the log.
        if (snapshot.diagnostics.Problems().empty())
            _output.ShowLastAttempt(snapshot.diagnostics.Attempt().id);
        else
            _output.ShowProblems();
        _viewport.ReleaseFocus();
        _showFailedOutput = false;
        result.cancelViewportInput = true;
    }
    const auto& attempt = snapshot.diagnostics.Attempt();
    const bool failed =
        attempt.result == EditorBuildResult::BuildFailed || attempt.result == EditorBuildResult::RefreshFailed;
    // A new failure brings its problems forward, unless that would interrupt typing or a focused scene; the status bar
    // keeps the count either way.
    if (failed && attempt.id != _failureAttempt) {
        _failureAttempt = attempt.id;
        const bool busy = _maximized || _consoleFocused || ImGui::GetIO().WantTextInput;
        if (attempt.id != snapshot.controlAttempt && !busy) _showFailedOutput = true;
    }
    EditorShellResult menu;
    // Help windows the palette asked for last frame
    menu.showShortcuts = std::exchange(_showShortcuts, false);
    menu.showAbout = std::exchange(_showAbout, false);
    const EditorActionContext context{.snapshot = snapshot,
                                      .actions = actions,
                                      .preferences = preferences,
                                      .fontSize = fontSize,
                                      .maximized = _maximized,
                                      .offerProjectList = _offerProjectList,
                                      .agentsDrawerOpen = _agentsDrawer.IsOpen(),
                                      .engineDirectory = _engineDirectory,
                                      .agentContextTarget = agentTarget.title,
                                      .sceneContextTarget = sceneTarget.title,
                                      .sceneContextUnavailable = sceneTarget.unavailable,
                                      .result = menu};
    const auto list = EditorActionList(context);
    _shell.Menu(context, list, _projectName, _gitBar);
    result.cancelViewportInput |= menu.dialogOpen;
    result.cancelViewportInput |= Shortcuts(list);
    if (std::exchange(_openQuickOpen, false)) menu.openQuick = true; // The toolbar's search box, last frame
    const bool palette = _palette.Draw(context);
    _showShortcuts = menu.showShortcuts;
    _showAbout = menu.showAbout;
    if (palette) {
        result.cancelViewportInput = true;
        _viewport.ReleaseFocus();
    }
    const auto* viewport = ImGui::GetMainViewport();
    const float menuHeight = ImGui::GetFrameHeight();
    EditorToolbarSearch search;
    if (menu.showActivity) ShowAgents(std::nullopt, preferences);
    // The drawer's keys: Ctrl+Shift+A opens it on the agent that waits (or closes it), the rest act inside it
    if (menu.agents.toggle) {
        if (_agentsDrawer.IsOpen())
            _agentsDrawer.Close();
        else
            ShowAgents(std::nullopt, preferences);
    }
    if (menu.agents.add) {
        if (!_agentsDrawer.IsOpen()) ShowAgents(std::nullopt, preferences);
        _agentsDrawer.AskAdd();
    }
    if (menu.agents.step != 0) _agentsDrawer.Step(menu.agents.step);
    if (menu.agents.chip) _agentsDrawer.Pick(*menu.agents.chip);
    if (menu.agents.switchView) _agentsDrawer.SwitchView();
    if (menu.agents.sendScene)
        if (const auto chosen = _agentsDrawer.ChosenSession()) {
            result.sendScene = true;
            result.sendSceneSession = chosen;
        }
    const float toolbarHeight =
        _shell.Toolbar(snapshot, actions, {viewport->Pos.x, viewport->Pos.y + menuHeight}, viewport->Size.x, _marking,
                       search, AgentStatus(snapshot), _agentActivity,
                       markTarget.title + (markTarget.unavailable.empty() ? "" : ". " + markTarget.unavailable));
    if (!_marking) _overlayStart.reset();
    if (_agentActivity.TakeOpenAgents()) {
        _agentsDrawer.Toggle(AgentStatusFocus());
        if (_agentsDrawer.IsOpen()) result.cancelViewportInput = true;
    }
    _palette.SetAnchor(search.centerX, search.top);
    _openQuickOpen = search.pressed;
    const float statusHeight =
        _shell.Status(snapshot, preferences, AgentStatus(snapshot),
                      {viewport->Pos.x, viewport->Pos.y + viewport->Size.y}, viewport->Size.x, menu, actions);
    if (_fixFailedBuild) {
        menu.agent = EditorAgentRequest::FixBuild;
        _fixFailedBuild = false;
    }
    if (menu.agent) HandleAgent(*menu.agent, snapshot, preferences, result);
    if (menu.toggleMaximize) SetMaximized(!_maximized, preferences);
    if (menu.toggleBottom || menu.bottomPanel) ToggleBottom(preferences, menu.bottomPanel);
    if (menu.showProblems) _showFailedOutput = true;
    if (menu.layout) ApplyLayout(*menu.layout, preferences);
    if (_maximized && oldVisibility != preferences.visible) SetMaximized(false, preferences);
    for (const auto panel : EditorBottomPanels)
        if (!oldVisibility[static_cast<std::size_t>(panel)] && preferences.Visible(panel)) {
            _focusBottom = panel;
            preferences.lastBottomPanel = panel;
        }
    if (menu.showProblems || menu.showActivity || menu.toggleBottom || menu.bottomPanel || menu.layout ||
        menu.toggleMaximize)
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
        _notes.Tick(ImGui::GetTime(), false);
        _consoleFocused = false; // No Console while the scene fills the workspace; the drawer's terminal is added below
        ImGui::SetNextWindowPos(workspacePosition);
        ImGui::SetNextWindowSize(workspaceSize);
        constexpr auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking |
                               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ToolColors().content);
        if (ImGui::Begin("Viewport###editor.maximized", nullptr, flags))
            result.interaction =
                _viewport.Draw(snapshot, actions, image, preferences.recent, preferences.showMetrics, true, !_marking);
        ImGui::End();
        ImGui::PopStyleColor();
    } else {
        constexpr auto flags = ImGuiWindowFlags_NoCollapse;
        bool viewportDrawn = false;
        bool notesFocused = false;
        bool notesDrawn = false;
        bool consoleDrawn = false;
        bool focusedBottomDrawn = false;
        for (const auto& panel : EditorPanels) {
            if (!preferences.Visible(panel.id)) continue;
            const bool focusViewport = _focusViewport && panel.id == EditorPanel::Viewport;
            if (focusViewport || (_focusBottom && *_focusBottom == panel.id) ||
                (_focusCatalog && panel.id == EditorPanel::Catalog) || (_focusNotes && panel.id == EditorPanel::Notes))
                ImGui::SetNextWindowFocus();
            if (focusViewport) ImGui::SetNextWindowCollapsed(false);
            const bool edgeToEdge = panel.id == EditorPanel::Console || panel.id == EditorPanel::Assets ||
                                    panel.id == EditorPanel::AudioPreview;
            if (edgeToEdge) ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, headerPadding);
            // The game sits on a neutral well: a tinted frame would change how its colors read.
            if (panel.id == EditorPanel::Viewport) ImGui::PushStyleColor(ImGuiCol_WindowBg, ToolColors().content);
            const bool drawPanel =
                ImGui::Begin(panel.window, &preferences.Visible(panel.id),
                             flags | (panel.id == EditorPanel::Viewport || panel.id == EditorPanel::Inspector ||
                                              panel.id == EditorPanel::Assets || panel.id == EditorPanel::AudioPreview
                                          ? ImGuiWindowFlags_NoScrollbar
                                          : 0));
            if (panel.id == EditorPanel::Viewport) ImGui::PopStyleColor();
            ImGui::PopStyleVar(); // Panel contents keep their regular control spacing.
            if (edgeToEdge) ImGui::PopStyleVar();
            if (drawPanel) {
                if (_focusBottom && *_focusBottom == panel.id) focusedBottomDrawn = true;
                if (panel.dock == EditorDockGroup::Bottom && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
                    preferences.lastBottomPanel = panel.id;
                switch (panel.id) {
                case EditorPanel::Catalog: _catalog.Draw(snapshot, actions, preferences); break;
                case EditorPanel::Viewport:
                    result.interaction = _viewport.Draw(snapshot, actions, image, preferences.recent,
                                                        preferences.showMetrics, false, !_marking);
                    viewportDrawn = preferences.Visible(panel.id);
                    break;
                case EditorPanel::Inspector:
                    _inspector.Draw(snapshot, actions);
                    if (const auto text = _inspector.TakeAgentText(); !text.empty())
                        result.agentProblem = SendToAgent(text, false, preferences);
                    break;
                case EditorPanel::Output:
                    _output.Draw(snapshot, actions, preferences, agentTarget.title);
                    if (const auto text = _output.TakeAgentText(); !text.empty())
                        result.agentProblem = SendToAgent(text, false, preferences, "Error context");
                    if (_output.TakeFixRequest()) _fixFailedBuild = true; // Handled next frame, with the other requests
                    break;
                case EditorPanel::Profiler: _profiler.Draw(snapshot, actions); break;
                case EditorPanel::Console : {
                    const auto request = _console.Draw(_sessions, _views, _agentsDrawer.Shown(_sessions.Infos()));
                    if (request.add) result.agentProblem = AddSession(*request.add, preferences);
                    if (request.showHere) _agentsDrawer.Close();
                }
                    consoleDrawn = true;
                    break;
                case EditorPanel::Assets:
                    _assets.Draw(snapshot, actions);
                    if (const auto path = _assets.TakeOpenNote(); !path.empty()) OpenNote(path, preferences);
                    if (const auto rename = _assets.TakeNoteRename()) _notes.Renamed(rename->first, rename->second);
                    break;
                case EditorPanel::Notes:
                    if (const auto paste = _notes.Draw(); !paste.empty()) PasteToConsole(paste, preferences);
                    notesFocused =
                        !ImGui::GetIO().AppFocusLost && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
                    notesDrawn = true;
                    break;
                case EditorPanel::AudioPreview: _audioPreview.Draw(snapshot, actions); break;
                case EditorPanel::Count       : break;
                }
            }
            ImGui::End();
        }
        _notes.Tick(ImGui::GetTime(), notesFocused);
        if (_focusNotes) {
            SelectPanelTab(EditorPanelInfo(EditorPanel::Notes).window);
            ImGui::SetWindowFocus(EditorPanelInfo(EditorPanel::Notes).window);
            if (notesDrawn) _focusNotes = false;
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
    if (_notes.Focused()) {
        result.cancelViewportInput = true;
        result.interaction = {};
        _viewport.ReleaseFocus();
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
    if (_agentSetup.Active()) {
        result.cancelViewportInput = true;
        const auto setup = _agentSetup.Draw(preferences.agent);
        if (setup == EditorAgentSetupResult::Started) {
            const auto text = std::exchange(_pendingAgentText, {});
            const bool submit = std::exchange(_pendingAgentSubmit, false);
            const auto pendingContext = std::exchange(_pendingAgentContext, {});
            result.agentProblem =
                text.empty() ? OpenAgent(preferences) : SendToAgent(text, submit, preferences, pendingContext);
        } else if (setup == EditorAgentSetupResult::Canceled) {
            _pendingAgentText.clear();
            _pendingAgentSubmit = false;
            if (!_pendingAgentContext.empty()) NotifyContext("Context paste canceled \xC2\xB7 Agent setup canceled");
            _pendingAgentContext.clear();
        }
    }
    GitOverlay(snapshot, actions, menu, workspacePosition, workspaceSize, preferences, result);
    AgentsOverlay(snapshot, actions, workspacePosition, workspaceSize, preferences, result);
    // Typing to an agent in the drawer counts as typing in a terminal, maximized or not: F5 and the like stay the
    // agent's
    _consoleFocused = _consoleFocused || _agentsDrawer.TerminalFocused();
    if (_marking)
        MarkOverlay(snapshot, menuHeight + toolbarHeight, result);
    else if (result.interaction.markDragging || (result.interaction.marking && result.interaction.hovered)) {
        auto hint = "Scene region \xC2\xB7 Paste mark to " + markTarget.title +
                    " \xC2\xB7 You submit\nThe scene pauses for marking. Esc / right-click cancels.";
        if (!markTarget.unavailable.empty()) hint += "\n" + markTarget.unavailable;
        DrawMarkHint(hint, workspacePosition, workspaceSize);
    }
    ContextNotice(workspacePosition, workspaceSize);
    if (result.cancelViewportInput) _viewport.ReleaseFocus();
    _viewportFocused = result.interaction.focused;
    return result;
}
