#pragma once
#include <vector>
#include "Editor/Ui/Commands/EditorCommandPalette.h"
#include "Editor/Ui/Panels/Assets/AssetsPanel.h"
#include "Editor/Ui/Panels/Audio/AudioPreviewPanel.h"
#include "Editor/Ui/Panels/Catalog/CatalogPanel.h"
#include "Editor/Ui/Panels/Console/ConsolePanel.h"
#include "Editor/Ui/Panels/Git/GitDiffView.h"
#include "Editor/Ui/Panels/Git/GitDrawer.h"
#include "Editor/Ui/Panels/Inspector/InspectorPanel.h"
#include "Editor/Ui/Panels/Output/OutputPanel.h"
#include "Editor/Ui/Panels/Viewport/ViewportPanel.h"
#include "Editor/Ui/Panels/Profiler/ProfilerPanel.h"
#include "Editor/Ui/Shell/EditorShell.h"
#include "Editor/Ui/Widgets/EditorAgentSetup.h"

struct EditorWorkspaceResult {
    EditorViewportInteraction interaction;
    bool quit = false;
    bool returnToProjectList = false;
    bool cancelViewportInput = false;
    std::optional<FloatRect> windowMark; // A finished mark outside the scene: window framebuffer pixels
    bool sendScene = false;              // Agent → Send scene: the host captures the frame and calls SendToAgent
    std::string agentProblem;            // For the log, e.g. the agent CLI was not found
};

class EditorWorkspace {
public:
    [[nodiscard]] EditorWorkspaceResult Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                             const ViewportImage& image, EditorPreferences& preferences, int& fontSize);
    void CaptureLayout(EditorPreferences& preferences) const;
    void ReleaseInput() { _viewport.ReleaseFocus(); }
    // File menu offers "Back to project list" (the host knows the launcher).
    void OfferProjectList(bool offer) { _offerProjectList = offer; }
    // Shown at the menu bar's far end.
    void SetProjectName(std::string name) { _projectName = std::move(name); }
    // Konsolun kabuğu bu klasörde başlar (proje kökü).
    void SetConsoleDirectory(const std::filesystem::path& directory) { _sessions.SetDirectory(directory); }
    void SetAssetRoots(const std::filesystem::path& engineDirectory, const std::filesystem::path& assetsDirectory) {
        _assets.SetRoots(engineDirectory, assetsDirectory);
        _audioPreview.SetRoots(engineDirectory, assetsDirectory);
        _palette.SetAssetRoots(engineDirectory, assetsDirectory);
    }
    // Shows the Console and pastes `text` (no Enter) into the agent session when there is one, else the selected
    // session, starting a shell if needed.
    void PasteToConsole(const std::string& text, EditorPreferences& preferences);
    // Shows the Console and pastes `text` into the agent session, starting the chosen agent CLI when needed; `submit`
    // presses Enter after it. Returns a problem for the log (empty when none).
    [[nodiscard]] std::string SendToAgent(const std::string& text, bool submit, EditorPreferences& preferences);
    void SetConsoleEnvironment(std::vector<std::pair<std::string, std::string>> environment) {
        _sessions.SetEnvironment(std::move(environment));
    }
    // The Console's sessions, also for the host (it tells the control endpoint which sessions live).
    [[nodiscard]] const ConsoleSessions& Sessions() const { return _sessions; }

private:
    void ShowConsole(EditorPreferences& preferences);
    // Opens (or selects) the agent session; returns a problem for the log (empty when none).
    [[nodiscard]] std::string OpenAgent(EditorPreferences& preferences);
    // The Console's add button: a shell, or a new session of an agent CLI (setup first when it is missing).
    [[nodiscard]] std::string AddSession(const ConsoleNewSession& request, EditorPreferences& preferences);
    void HandleAgent(EditorAgentRequest request, const EditorSnapshot& snapshot, EditorPreferences& preferences,
                     EditorWorkspaceResult& result);
    [[nodiscard]] EditorAgentStatus AgentStatus(const EditorSnapshot& snapshot) const;
    void SetMaximized(bool maximized, EditorPreferences& preferences);
    void ApplyLayout(EditorWorkspaceLayout layout, EditorPreferences& preferences);
    void ToggleBottom(EditorPreferences& preferences, std::optional<EditorPanel> panel);
    // The Mark toggle's layer over everything below the toolbar: a region wholly on the scene becomes a scene
    // mark, anything else a window mark.
    void MarkOverlay(const EditorSnapshot& snapshot, const EditorUiActions& actions, float top,
                     EditorWorkspaceResult& result);
    [[nodiscard]] bool Shortcuts(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                 EditorShellResult& result) const;
    void DockHost(ImVec2 position, ImVec2 size, bool reset, const EditorPreferences& preferences);
    // The Git drawer and the file open in it, above the panels: they never change the layout underneath.
    void GitOverlay(const EditorSnapshot& snapshot, const EditorUiActions& actions, const EditorShellResult& menu,
                    ImVec2 position, ImVec2 size, EditorPreferences& preferences, EditorWorkspaceResult& result);
    CatalogPanel _catalog;
    ViewportPanel _viewport;
    InspectorPanel _inspector;
    OutputPanel _output;
    ProfilerPanel _profiler;
    ConsoleSessions _sessions;
    ConsolePanel _console;
    AssetsPanel _assets;
    AudioPreviewPanel _audioPreview;
    EditorAgentActivity _agentActivity;
    EditorGitBar _gitBar;
    GitDrawer _gitDrawer;
    GitDiffView _gitDiff;
    EditorShell _shell;
    EditorCommandPalette _palette;
    EditorAgentSetup _agentSetup;
    std::string _pendingAgentText; // Context waits for the developer to accept setup.
    bool _pendingAgentSubmit = false;
    std::string _restoreLayout;
    bool _initialized = false;
    bool _maximized = false;
    bool _offerProjectList = false;
    std::string _projectName;
    bool _pendingToggle = false;
    bool _focusCatalog = false;
    bool _focusViewport = true;
    bool _marking = false; // The toolbar's Mark toggle
    std::optional<ImVec2> _overlayStart;
    bool _consoleFocused = false;  // önceki karede klavye konsoldaydı
    bool _viewportFocused = false; // The scene had the keyboard last frame
    std::optional<EditorPanel> _focusBottom;
    std::uint64_t _failureAttempt = 0;
    bool _showFailedOutput = false;
    bool _fixFailedBuild = false; // Problems' "Fix all with agent"
    bool _openQuickOpen = false;  // The toolbar search box was pressed
    ImGuiID _centerDock = 0;
    int _ignoreLayoutChanges = 0;
    bool _resetDock = false;
    std::array<bool, static_cast<std::size_t>(EditorPanel::Count)> _dockOnOpen{};
};
