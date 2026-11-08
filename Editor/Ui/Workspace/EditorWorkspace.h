#pragma once
#include <chrono>
#include <vector>
#include "Editor/Ui/Commands/EditorCommandPalette.h"
#include "Editor/Ui/Panels/Assets/AssetsPanel.h"
#include "Editor/Ui/Panels/Agents/AgentsDrawer.h"
#include "Editor/Ui/Panels/Audio/AudioPreviewPanel.h"
#include "Editor/Ui/Panels/Catalog/CatalogPanel.h"
#include "Editor/Ui/Panels/Console/ConsolePanel.h"
#include "Editor/Ui/Panels/Git/GitDiffView.h"
#include "Editor/Ui/Panels/Git/GitDrawer.h"
#include "Editor/Ui/Panels/Inspector/InspectorPanel.h"
#include "Editor/Ui/Panels/Output/OutputPanel.h"
#include "Editor/Ui/Panels/Notes/NotesPanel.h"
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
    bool sendScene = false;              // Paste scene context: the host captures the frame and calls SendToAgent
    std::optional<std::uint64_t> sendSceneSession; // Agents drawer: send to this Console session
    std::string agentProblem;                      // For the log, e.g. the agent CLI was not found
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
        _engineDirectory = engineDirectory;
    }
    void SetNotesProject(const std::filesystem::path& directory) {
        _notes.SetProject(directory);
        _assets.SetNotesProject(directory);
    }
    void OpenNote(const std::filesystem::path& path, EditorPreferences& preferences);
    bool FlushNotes() { return _notes.Flush(); }
    void CloseNotes() { _notes.Shutdown(); }
    // Pastes `text` (no Enter) into the agent session, shown in the Agents drawer, when there is one; else into the
    // Console's selected session, starting a shell if needed.
    void PasteToConsole(const std::string& text, EditorPreferences& preferences, const std::string& context = {});
    // Pastes `text` into the agent session, shown in the Agents drawer, starting the chosen agent CLI when needed;
    // `submit` presses Enter after it. Returns a problem for the log (empty when none).
    [[nodiscard]] std::string SendToAgent(const std::string& text, bool submit, EditorPreferences& preferences,
                                          const std::string& context = {});
    // Sends to an existing agent session chosen in the Agents drawer; it never starts another session.
    [[nodiscard]] std::string SendToSession(std::uint64_t id, const std::string& text, bool submit,
                                            const std::string& context = {});
    // A short, non-modal result, also visible with the viewport and drawer closed.
    void NotifyContext(std::string text, bool failed = false);
    void SetConsoleEnvironment(std::vector<std::pair<std::string, std::string>> environment) {
        _sessions.SetEnvironment(std::move(environment));
    }
    // The Console's sessions, also for the host (it tells the control endpoint which sessions live).
    [[nodiscard]] const ConsoleSessions& Sessions() const { return _sessions; }

private:
    void ShowConsole(EditorPreferences& preferences);
    // The Agents drawer, on `session` (an agent session) or the one the strip speaks of.
    void ShowAgents(std::optional<std::uint64_t> session, EditorPreferences& preferences);
    // The drawer behind the strip, right of the Git drawer when that is open.
    void AgentsOverlay(const EditorSnapshot& snapshot, const EditorUiActions& actions, ImVec2 position, ImVec2 size,
                       EditorPreferences& preferences, EditorWorkspaceResult& result);
    // Opens (or selects) the agent session; returns a problem for the log (empty when none).
    [[nodiscard]] std::string OpenAgent(EditorPreferences& preferences);
    // The Console's add button: a shell, or a new session of an agent CLI (setup first when it is missing).
    // `inDrawer`: asked from the Agents drawer, which then shows the new session instead of the Console.
    [[nodiscard]] std::string AddSession(const ConsoleNewSession& request, EditorPreferences& preferences,
                                         bool inDrawer = false);
    void HandleAgent(EditorAgentRequest request, const EditorSnapshot& snapshot, EditorPreferences& preferences,
                     EditorWorkspaceResult& result);
    [[nodiscard]] EditorAgentStatus AgentStatus(const EditorSnapshot& snapshot) const;
    // The session the strip speaks of and the drawer opens on: one waiting for the developer, else the agent target.
    [[nodiscard]] std::optional<std::uint64_t> AgentStatusFocus() const;
    void SetMaximized(bool maximized, EditorPreferences& preferences);
    void ApplyLayout(EditorWorkspaceLayout layout, EditorPreferences& preferences);
    void ToggleBottom(EditorPreferences& preferences, std::optional<EditorPanel> panel);
    // The Mark toggle's layer over everything below the toolbar: a region wholly on the scene becomes a scene
    // mark, anything else a window mark.
    void MarkOverlay(const EditorSnapshot& snapshot, float top, EditorWorkspaceResult& result);
    void ContextNotice(ImVec2 position, ImVec2 size) const;
    // Runs the action bound to each key pressed (EditorKeyBindings); true when a key was the editor's.
    [[nodiscard]] bool Shortcuts(std::span<const EditorAction> list) const;
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
    ConsoleViews _views;
    AgentsDrawer _agentsDrawer;
    ConsolePanel _console;
    AssetsPanel _assets;
    NotesPanel _notes;
    bool _focusNotes = false;
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
    std::string _pendingAgentContext;
    std::string _contextNotice;
    bool _contextNoticeFailed = false;
    std::chrono::steady_clock::time_point _contextNoticeAt;
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
    bool _fixFailedBuild = false; // Problems' "Ask agent to fix build"
    bool _openQuickOpen = false;  // The toolbar search box was pressed
    bool _showShortcuts = false;  // Help windows asked for from the palette, opened next frame
    bool _showAbout = false;
    std::filesystem::path _engineDirectory; // Help → Editor guide
    ImGuiID _centerDock = 0;
    int _ignoreLayoutChanges = 0;
    bool _resetDock = false;
    std::array<bool, static_cast<std::size_t>(EditorPanel::Count)> _dockOnOpen{};
};
