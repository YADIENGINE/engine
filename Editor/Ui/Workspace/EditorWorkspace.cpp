#include "Editor/Ui/Workspace/EditorWorkspace.h"
#include <imgui_internal.h>
#include <algorithm>

namespace {
constexpr const char* CatalogWindow = "Project###editor.catalog";
constexpr const char* PreviewWindow = "View###editor.viewport";
constexpr const char* InspectorWindow = "Inspector###editor.inspector";
constexpr const char* OutputWindow = "Output###editor.output";
constexpr const char* ProfilerWindow = "Profiler###editor.profiler";
constexpr float CatalogWidthInFonts = 18.0f;
constexpr float InspectorWidthInFonts = 24.0f;
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
} // namespace
void EditorWorkspace::CaptureLayout(EditorPreferences& preferences) const {
    if (!_initialized || _maximized) return;
    std::size_t size = 0;
    const char* bytes = ImGui::SaveIniSettingsToMemory(&size);
    preferences.docking.assign(bytes, size);
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
}
void EditorWorkspace::DockHost(ImVec2 position, ImVec2 size, bool reset) {
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
        const ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.24f, nullptr, &center);
        const float catalogRatio = std::clamp(ImGui::GetFontSize() * CatalogWidthInFonts / size.x, 0.20f, 0.30f);
        const float remainingWidth = std::max(1.0f, size.x * (1.0f - catalogRatio));
        const float inspectorRatio =
            std::clamp(ImGui::GetFontSize() * InspectorWidthInFonts / remainingWidth, 0.27f, 0.45f);
        const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, catalogRatio, nullptr, &center);
        const ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, inspectorRatio, nullptr, &center);
        ImGui::DockBuilderDockWindow(CatalogWindow, left);
        ImGui::DockBuilderDockWindow(PreviewWindow, center);
        ImGui::DockBuilderDockWindow(InspectorWindow, right);
        ImGui::DockBuilderDockWindow(OutputWindow, bottom);
        ImGui::DockBuilderDockWindow(ProfilerWindow, bottom);
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
    if (!_initialized) {
        if (!preferences.docking.empty())
            ImGui::LoadIniSettingsFromMemory(preferences.docking.data(), preferences.docking.size());
        preferences.Visible(EditorPanel::Catalog) = true;
        preferences.Visible(EditorPanel::Viewport) = true;
        _initialized = true;
    }
    if (_pendingToggle) {
        SetMaximized(!_maximized, preferences);
        _pendingToggle = false;
    }
    const auto oldVisibility = preferences.visible;
    const auto menu = _shell.Menu(snapshot, actions, preferences, fontSize, _maximized);
    if (!oldVisibility[static_cast<std::size_t>(EditorPanel::Output)] && preferences.Visible(EditorPanel::Output))
        _focusOutput = true;
    if (menu.toggleMaximize) SetMaximized(!_maximized, preferences);
    if (_maximized && oldVisibility != preferences.visible) SetMaximized(false, preferences);
    if (menu.restoreLayout) {
        SetMaximized(false, preferences);
        preferences.visible.fill(true);
    }
    const auto* viewport = ImGui::GetMainViewport();
    const float menuHeight = ImGui::GetFrameHeight();
    const float statusHeight =
        _shell.Status(snapshot, {viewport->Pos.x, viewport->Pos.y + viewport->Size.y}, viewport->Size.x);
    const ImVec2 workspacePosition{viewport->Pos.x, viewport->Pos.y + menuHeight};
    const ImVec2 workspaceSize{viewport->Size.x, std::max(1.0f, viewport->Size.y - menuHeight - statusHeight)};
    // Docking ignores SetNextWindowSizeConstraints. Scope the native minimum to the workspace
    // so docked/floating panels share it without forcing the status bar to grow.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, PanelMinimumSize(workspaceSize));
    DockHost(workspacePosition, workspaceSize, menu.restoreLayout);
    EditorWorkspaceResult result;
    result.quit = menu.quit;
    if (_maximized) {
        ImGui::SetNextWindowPos(workspacePosition);
        ImGui::SetNextWindowSize(workspaceSize);
        // An interactive preview must appear above the dock host. HostFlags creates windows at the back.
        constexpr ImGuiWindowFlags PreviewFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                                  ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
                                                  ImGuiWindowFlags_NoScrollbar;
        if (ImGui::Begin("Preview###editor.maximized", nullptr, PreviewFlags))
            result.interaction = _viewport.Draw(snapshot, actions, image, preferences.showMetrics, true);
        ImGui::End();
    } else {
        constexpr ImGuiWindowFlags PanelFlags = ImGuiWindowFlags_NoCollapse;
        if (preferences.Visible(EditorPanel::Catalog)) {
            if (ImGui::Begin(CatalogWindow, &preferences.Visible(EditorPanel::Catalog), PanelFlags))
                _catalog.Draw(snapshot, actions, preferences);
            ImGui::End();
        }
        bool viewportDrawn = false;
        if (preferences.Visible(EditorPanel::Viewport)) {
            if (ImGui::Begin(PreviewWindow, &preferences.Visible(EditorPanel::Viewport),
                             PanelFlags | ImGuiWindowFlags_NoScrollbar)) {
                result.interaction = _viewport.Draw(snapshot, actions, image, preferences.showMetrics, false);
                viewportDrawn = preferences.Visible(EditorPanel::Viewport);
            }
            ImGui::End();
        }
        if (!viewportDrawn) {
            _viewport.ReleaseFocus();
            result.interaction = {};
        }
        if (preferences.Visible(EditorPanel::Inspector)) {
            if (ImGui::Begin(InspectorWindow, &preferences.Visible(EditorPanel::Inspector), PanelFlags))
                _inspector.Draw(snapshot, actions);
            ImGui::End();
        }
        if (preferences.Visible(EditorPanel::Output)) {
            if (_focusOutput) {
                ImGui::SetNextWindowFocus();
                _focusOutput = false;
            }
            if (ImGui::Begin(OutputWindow, &preferences.Visible(EditorPanel::Output), PanelFlags))
                _output.Draw(snapshot, actions, preferences);
            ImGui::End();
        }
        if (preferences.Visible(EditorPanel::Profiler)) {
            const auto* output = ImGui::FindWindowByName(OutputWindow);
            ImGui::SetNextWindowDockID(output ? output->DockId : 0, ImGuiCond_FirstUseEver);
            if (!oldVisibility[static_cast<std::size_t>(EditorPanel::Profiler)]) ImGui::SetNextWindowFocus();
            if (ImGui::Begin(ProfilerWindow, &preferences.Visible(EditorPanel::Profiler), PanelFlags))
                _profiler.Draw(snapshot, actions);
            ImGui::End();
        }
        if (opening) {
            SelectPanelTab(CatalogWindow);
            SelectPanelTab(PreviewWindow);
            ImGui::SetWindowFocus(PreviewWindow);
        }
    }
    ImGui::PopStyleVar();
    if (result.interaction.toggleMaximize) _pendingToggle = true;
    if (!_maximized && ImGui::GetIO().WantSaveIniSettings) CaptureLayout(preferences);
    return result;
}
