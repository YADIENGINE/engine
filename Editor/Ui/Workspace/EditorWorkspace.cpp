#include "Editor/Ui/Workspace/EditorWorkspace.h"
#include <imgui_internal.h>
#include <algorithm>
#include <string_view>
#include "Editor/Ui/Commands/EditorCommands.h"

namespace {
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
    return preferences.visible[static_cast<std::size_t>(EditorPanel::Output)] ||
           preferences.visible[static_cast<std::size_t>(EditorPanel::Profiler)];
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
        _resetDock = true;
    }
    _ignoreLayoutChanges = 3;
    _viewport.ReleaseFocus();
}
void EditorWorkspace::ToggleBottom(EditorPreferences& preferences, std::optional<EditorPanel> panel) {
    const bool close = BottomVisible(preferences) && (!panel || *panel == preferences.lastBottomPanel);
    if (panel) preferences.lastBottomPanel = *panel;
    preferences.Visible(EditorPanel::Output) = false;
    preferences.Visible(EditorPanel::Profiler) = false;
    if (!close) {
        preferences.Visible(preferences.lastBottomPanel) = true;
        _focusBottom = preferences.lastBottomPanel;
    }
}
bool EditorWorkspace::Shortcuts(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                EditorShellResult& result) const {
    constexpr auto route = ImGuiInputFlags_RouteGlobal;
    // Register stable global routes even when a popup/text field temporarily blocks execution.
    const bool palette = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P, route);
    const bool catalog = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_P, route);
    const bool bottom = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_J, route);
    const bool primary = ImGui::Shortcut(ImGuiKey_F5, route);
    const auto& io = ImGui::GetIO();
    const bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    const bool blocked = popup || io.WantTextInput;
    bool consumed = false;
    if (palette && !popup) {
        result.openCommands = true;
        consumed = true;
    }
    if (catalog && !popup) {
        result.focusCatalog = true;
        consumed = true;
    }
    if (bottom && !blocked) {
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
        if (BottomVisible(preferences))
            groups[static_cast<std::size_t>(EditorDockGroup::Bottom)] =
                ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.30f, nullptr, &center);
        const bool catalog = preferences.visible[static_cast<std::size_t>(EditorPanel::Catalog)];
        if (catalog)
            groups[static_cast<std::size_t>(EditorDockGroup::Left)] =
                ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
        if (preferences.visible[static_cast<std::size_t>(EditorPanel::Inspector)] ||
            preferences.visible[static_cast<std::size_t>(EditorPanel::Tuning)]) {
            const float ratio = preferences.layout == EditorWorkspaceLayout::Inspect ? 0.32f : 0.24f;
            groups[static_cast<std::size_t>(EditorDockGroup::Right)] =
                ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, ratio / (catalog ? 0.80f : 1.0f), nullptr, &center);
        }
        _centerDock = center;
        groups[static_cast<std::size_t>(EditorDockGroup::Center)] = center;
        for (const auto& panel : EditorPanels) {
            const auto index = static_cast<std::size_t>(panel.id);
            _dockOnOpen[index] = !preferences.visible[index];
            if (preferences.visible[index])
                ImGui::DockBuilderDockWindow(panel.window, groups[static_cast<std::size_t>(panel.dock)]);
        }
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
        for (const auto& peer : EditorPanels)
            if (peer.id != panel.id && peer.dock == panel.dock) {
                const auto peerDock = SavedPanelDock(peer);
                if (peerDock && ImGui::DockBuilderGetNode(peerDock)) target = peerDock;
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
                                            const EditorPreviewImage& image, EditorPreferences& preferences,
                                            int& fontSize) {
    const bool opening = !_initialized;
    if (opening) {
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
        result.cancelPreviewInput = true;
    }
    const auto& attempt = snapshot.diagnostics.Attempt();
    const bool failed =
        attempt.result == EditorBuildResult::BuildFailed || attempt.result == EditorBuildResult::RefreshFailed;
    constexpr const char* FailurePopup = "Build & Refresh failed";
    if (failed && attempt.id != _failureAttempt &&
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
        result.cancelPreviewInput = true;
    }
    auto menu = _shell.Menu(snapshot, actions, preferences, fontSize, _maximized);
    result.cancelPreviewInput |= Shortcuts(snapshot, actions, menu);
    if (_palette.Draw(snapshot, actions, preferences, _maximized, menu)) {
        result.cancelPreviewInput = true;
        _viewport.ReleaseFocus();
    }
    const auto* viewport = ImGui::GetMainViewport();
    const float menuHeight = ImGui::GetFrameHeight();
    const float toolbarHeight =
        _shell.Toolbar(snapshot, actions, {viewport->Pos.x, viewport->Pos.y + menuHeight}, viewport->Size.x);
    const float statusHeight = _shell.Status(
        snapshot, preferences, {viewport->Pos.x, viewport->Pos.y + viewport->Size.y}, viewport->Size.x, menu);
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
    for (const auto panel : {EditorPanel::Output, EditorPanel::Profiler})
        if (!oldVisibility[static_cast<std::size_t>(panel)] && preferences.Visible(panel)) {
            _focusBottom = panel;
            preferences.lastBottomPanel = panel;
        }
    if (menu.focusCatalog || menu.toggleBottom || menu.bottomPanel || menu.layout || menu.toggleMaximize)
        result.cancelPreviewInput = true;
    const ImVec2 workspacePosition{viewport->Pos.x, viewport->Pos.y + menuHeight + toolbarHeight};
    const ImVec2 workspaceSize{viewport->Size.x,
                               std::max(1.0f, viewport->Size.y - menuHeight - toolbarHeight - statusHeight)};
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, PanelMinimumSize(workspaceSize));
    DockHost(workspacePosition, workspaceSize, _resetDock, preferences);
    _resetDock = false;
    result.quit = menu.quit;
    if (_maximized) {
        ImGui::SetNextWindowPos(workspacePosition);
        ImGui::SetNextWindowSize(workspaceSize);
        constexpr auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking |
                               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;
        if (ImGui::Begin("Preview###editor.maximized", nullptr, flags))
            result.interaction = _viewport.Draw(snapshot, actions, image, preferences.showMetrics, true);
        ImGui::End();
    } else {
        constexpr auto flags = ImGuiWindowFlags_NoCollapse;
        bool viewportDrawn = false;
        for (const auto& panel : EditorPanels) {
            if (!preferences.Visible(panel.id)) continue;
            if ((_focusBottom && *_focusBottom == panel.id) || (_focusCatalog && panel.id == EditorPanel::Catalog))
                ImGui::SetNextWindowFocus();
            if (ImGui::Begin(panel.window, &preferences.Visible(panel.id),
                             flags | (panel.id == EditorPanel::Viewport || panel.id == EditorPanel::Inspector
                                          ? ImGuiWindowFlags_NoScrollbar
                                          : 0))) {
                if (panel.dock == EditorDockGroup::Bottom && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
                    preferences.lastBottomPanel = panel.id;
                switch (panel.id) {
                case EditorPanel::Catalog: _catalog.Draw(snapshot, actions, preferences); break;
                case EditorPanel::Viewport:
                    result.interaction = _viewport.Draw(snapshot, actions, image, preferences.showMetrics, false);
                    viewportDrawn = preferences.Visible(panel.id);
                    break;
                case EditorPanel::Inspector: _inspector.Draw(snapshot, actions); break;
                case EditorPanel::Output   : _output.Draw(snapshot, actions, preferences); break;
                case EditorPanel::Profiler : _profiler.Draw(snapshot, actions); break;
                case EditorPanel::Tuning   : _tuning.Draw(snapshot, actions); break;
                case EditorPanel::Count    : break;
                }
            }
            ImGui::End();
        }
        _focusBottom.reset();
        _focusCatalog = false;
        if (!viewportDrawn) {
            _viewport.ReleaseFocus();
            result.interaction = {};
        }
        if (opening && preferences.Visible(EditorPanel::Viewport)) {
            SelectPanelTab(EditorPanelInfo(EditorPanel::Viewport).window);
            ImGui::SetWindowFocus(EditorPanelInfo(EditorPanel::Viewport).window);
        }
    }
    ImGui::PopStyleVar();
    if (preferences.visible != oldVisibility && !menu.layout) {
        preferences.layout = EditorWorkspaceLayout::Custom;
        result.cancelPreviewInput = true;
    }
    if (result.interaction.toggleMaximize) {
        _pendingToggle = true;
        result.cancelPreviewInput = true;
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
        ImGui::TextWrapped("Select text with the mouse, then Ctrl+C. Ctrl+A selects all text.");
        ImGui::InputTextMultiline("##failure-detail", _failureDetail.data(), _failureDetail.size(),
                                  {-1, ImGui::GetTextLineHeightWithSpacing() * 8}, ImGuiInputTextFlags_ReadOnly);
        ImGui::Separator();
        if (ImGui::Button("Copy")) ImGui::SetClipboardText(_failureDetail.data());
        ImGui::SameLine();
        if (ImGui::Button("Show Output")) {
            _showFailedOutput = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        result.cancelPreviewInput = true;
    }
    if (result.cancelPreviewInput) _viewport.ReleaseFocus();
    return result;
}
