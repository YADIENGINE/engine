#pragma once
#include "Editor/Ui/Panels/Catalog/CatalogPanel.h"
#include "Editor/Ui/Panels/Inspector/InspectorPanel.h"
#include "Editor/Ui/Panels/Output/OutputPanel.h"
#include "Editor/Ui/Panels/Viewport/ViewportPanel.h"
#include "Editor/Ui/Panels/Profiler/ProfilerPanel.h"
#include "Editor/Ui/Shell/EditorShell.h"

struct EditorWorkspaceResult {
    EditorViewportInteraction interaction;
    bool quit = false;
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
    void DockHost(ImVec2 position, ImVec2 size, bool reset);
    CatalogPanel _catalog;
    ViewportPanel _viewport;
    InspectorPanel _inspector;
    OutputPanel _output;
    ProfilerPanel _profiler;
    EditorShell _shell;
    std::string _restoreLayout;
    bool _initialized = false;
    bool _maximized = false;
    bool _pendingToggle = false;
    bool _focusOutput = false;
};
