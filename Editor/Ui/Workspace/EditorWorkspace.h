#pragma once
#include <vector>
#include "Editor/Ui/Commands/EditorCommandPalette.h"
#include "Editor/Ui/Panels/Catalog/CatalogPanel.h"
#include "Editor/Ui/Panels/Inspector/InspectorPanel.h"
#include "Editor/Ui/Panels/Output/OutputPanel.h"
#include "Editor/Ui/Panels/Viewport/ViewportPanel.h"
#include "Editor/Ui/Panels/Profiler/ProfilerPanel.h"
#include "Editor/Ui/Panels/Tuning/TuningPanel.h"
#include "Editor/Ui/Shell/EditorShell.h"

struct EditorWorkspaceResult {
    EditorViewportInteraction interaction;
    bool quit = false;
    bool cancelPreviewInput = false;
};

class EditorWorkspace {
public:
    [[nodiscard]] EditorWorkspaceResult Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                             const EditorPreviewImage& image, EditorPreferences& preferences,
                                             int& fontSize);
    void CaptureLayout(EditorPreferences& preferences) const;
    void ReleaseInput() { _viewport.ReleaseFocus(); }

private:
    void SetMaximized(bool maximized, EditorPreferences& preferences);
    void ApplyLayout(EditorWorkspaceLayout layout, EditorPreferences& preferences);
    void ToggleBottom(EditorPreferences& preferences, std::optional<EditorPanel> panel);
    [[nodiscard]] bool Shortcuts(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                 EditorShellResult& result) const;
    void DockHost(ImVec2 position, ImVec2 size, bool reset, const EditorPreferences& preferences);
    CatalogPanel _catalog;
    ViewportPanel _viewport;
    InspectorPanel _inspector;
    OutputPanel _output;
    ProfilerPanel _profiler;
    TuningPanel _tuning;
    EditorShell _shell;
    EditorCommandPalette _palette;
    std::string _restoreLayout;
    bool _initialized = false;
    bool _maximized = false;
    bool _pendingToggle = false;
    bool _focusCatalog = false;
    std::optional<EditorPanel> _focusBottom;
    std::uint64_t _failureAttempt = 0;
    std::vector<char> _failureDetail;
    bool _showFailedOutput = false;
    ImGuiID _centerDock = 0;
    int _ignoreLayoutChanges = 0;
    bool _resetDock = false;
    std::array<bool, static_cast<std::size_t>(EditorPanel::Count)> _dockOnOpen{};
};
