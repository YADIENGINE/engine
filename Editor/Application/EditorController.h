#pragma once
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <vector>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Application/WorkerBuild.h"
#include "Editor/Application/WorkerProcess.h"
#include "Editor/Internals/Session/EditorProject.h"
#include "Editor/Internals/Session/RefreshState.h"
#include "Editor/Internals/Session/TuningBaselines.h"
#include "Editor/Internals/Session/TuningWorkspace.h"
#include "Editor/Internals/Session/WorkerSnapshots.h"

// The worker's answer to the last Sync(): every command sent before it had applied. `frames` counts the frames received
// since; the second one was drawn after the answer.
struct EditorSync {
    std::uint64_t request = 0;
    double seconds = 0.0;
    bool paused = false;
    std::uint64_t frames = 0;
};

// The editor session: the catalog, the selected scene with its parameters, and the workers behind it.
// Workers run from snapshots of the build (WorkerSnapshots), so a build never waits for a worker to exit. At most one
// worker is active; Build & Refresh starts the new build as a replacement beside it and swaps once that is ready.
// Stopped and replaced workers retire: they shut down in the background while the session moves on.
class EditorController {
public:
    explicit EditorController(const std::filesystem::path& projectFile);
    ~EditorController();
    void Tick();
    void Select(const std::string& id);
    void Parameter(const std::string& id, const SceneValue& value);
    void Action(const std::string& id);
    void Playback(double speed, bool step);
    void SetPaused(bool paused);
    // Has the worker write what the device plays to a WAV file.
    void CaptureAudio(const std::string& file, double seconds);
    // Pauses and advances the playback clock to `seconds`, or by `seconds` when relative (replaying from 0 to go back).
    void Seek(double seconds, bool relative = false);
    // The same in the open simulation's steps.
    void SeekStep(std::uint64_t step, bool relative = false);
    // Loads or saves a replay of the open simulation ("load", "save"); the answer is in SimulationState().
    void Replay(const std::string& action, const std::string& path);
    // Asks the ready worker to confirm the commands sent so far; the answer arrives in LastSync(). Returns its request.
    [[nodiscard]] std::uint64_t Sync();
    [[nodiscard]] const EditorSync& LastSync() const { return _sync; }
    // Playback seconds since open / reset, as last reported by the worker.
    [[nodiscard]] double Seconds() const { return _seconds; }
    // The worker's report of the open scene's simulation (WorkerSimulation::State); null without one.
    [[nodiscard]] const nlohmann::json& SimulationState() const { return _simulationState; }
    [[nodiscard]] SceneParameters Values(const std::string& id) const;
    // A change made through the control endpoint: logged, shown briefly over the viewport and, unless the caller
    // records its own detail, kept in the session's activity.
    void Notify(const std::string& text, bool record = true);
    // One entry of the activity (the toolbar's agent strip); the oldest leave after ControlEventsKept. The entry
    // carries the control source's session.
    void RecordControl(EditorControlEvent event);
    // The session whose control request is being run or answered (EditorControlRequest::session).
    void SetControlSource(std::string session) { _controlSource = std::move(session); }
    // Opens a file from build output at a line: VS Code when it is on PATH, otherwise the file's default application.
    // A relative path counts from the project root.
    void OpenSource(const std::string& file, int line);
    // Opens a local file or directory in its default Windows application.
    void OpenPath(const std::filesystem::path& path);
    // The current Build & Refresh attempt was started through the control endpoint: its client gets the result, so a
    // failure does not open the blocking dialog.
    void MarkControlAttempt() { _controlAttempt = _output.Attempt().id; }
    void Input(const DeviceInput& input);
    void Profile(EditorProfilerCommand command, const Profiler::Recipe& recipe, const std::string& file);
    void Tuning(const TuningEditRequest& request);
    void InspectTuning(const std::filesystem::path& file) { _inspectedTuning = file; }
    void HostTransfer(double ms) { _hostTransferMs = ms; }
    void Log(const std::string& text, EditorLogLevel level = EditorLogLevel::Info,
             EditorLogSource source = EditorLogSource::Editor);
    void Execute(EditorCommand command);
    void ClearOutput() { _output.Clear(); }
    void AcknowledgeOutput() { _output.Acknowledge(); }
    void OpenOutputLog(bool attempt);
    [[nodiscard]] EditorSnapshot Snapshot() const;
    [[nodiscard]] const EditorDiagnostics& Diagnostics() const { return _output; }
    [[nodiscard]] std::string ProjectKey() const;
    [[nodiscard]] const std::optional<EditorProject>& Project() const { return _project; }
    [[nodiscard]] std::optional<std::string> TakeOpenedScene();
    void Shutdown();
    // Source control is changing files: builds wait.
    void SetSourcesBusy(bool busy) { _sourcesBusy = busy; }
    [[nodiscard]] std::uint64_t Generation() const { return _generation; }
    // The frame not yet shown, once; LatestFrame keeps the last received one of the active generation.
    [[nodiscard]] std::shared_ptr<const ViewportPixels> TakeFrame();
    [[nodiscard]] std::shared_ptr<const ViewportPixels> LatestFrame() const { return _latestFrame; }

private:
    // The worker that will replace the active one, with what it brings until the swap.
    struct Replacement {
        std::unique_ptr<WorkerProcess> worker;
        std::uint64_t generation = 0;
        WorkerOpenState state;
        std::optional<std::vector<SceneDefinition>> catalog;
        std::optional<std::vector<AudioUse>> audio;
        std::map<std::string, SceneParameters> accepted;
        bool selectionMissing = false; // The new catalog lacks the selected scene: it only refreshes the catalog
        std::vector<WorkerTuningStatus> tuning;
        std::optional<std::vector<std::string>> tuningUsed;
    };

    void Open();
    void Stop();
    void Pause();
    void Reset();
    void Refresh();
    void BuildAndPlay();
    void Cancel();
    [[nodiscard]] const SceneDefinition* Selected() const;
    [[nodiscard]] EditorPhase Phase() const;
    [[nodiscard]] bool Starting() const;
    [[nodiscard]] bool Busy() const;
    [[nodiscard]] WorkerOpenState OpenState() const;
    [[nodiscard]] std::map<std::string, SceneParameters> RestoreParameters(const std::vector<SceneDefinition>& catalog);
    [[nodiscard]] std::uint64_t NextGeneration() { return ++_generations; }

    void Launch(WorkerProcessMode mode);
    void Retire(std::unique_ptr<WorkerProcess> worker);
    void StopActive(const std::string& status);
    void Invalidate(std::uint64_t generation);
    void Send(const std::string& type, nlohmann::json data = nlohmann::json::object());

    void TickActive();
    void Receive(const WorkerMessage& message);
    void ApplyReady(const WorkerMessage& message);
    void ActiveExited(WorkerProcessMode mode, unsigned long code);
    // Worker'ın tuning raporu: panelin durumu ve Output (ilk başarılı okuma hariç).
    void ReceiveTuning(const WorkerTuningStatus& status);
    void ReceiveTuningUsed(const std::vector<std::string>& sources);
    // Asks the running scene to reread a file the editor just wrote.
    void ReloadTuning(const std::filesystem::path& source);
    void TickTuning();

    void TickBuild();
    void BuildFinished(const WorkerBuildExit& exit);
    void StartReplacement();
    // With no scene open, Build & Refresh ends with the catalog worker that reads the new build.
    void EndCatalogRefresh(EditorBuildResult result);
    void TickReplacement();
    void ReceiveReplacement(const WorkerMessage& message);
    void CommitReplacement(const WorkerMessage& ready);
    void CommitCatalogOnly();
    void AbandonReplacement(const std::string& reason, EditorBuildResult result);
    void TickRetired();

    // A fault of the active worker stops only the scene; a fault of Build & Refresh leaves the scene running.
    void FailScene(const std::string& text);
    void FailRefresh(const std::string& text);
    void FinishFailedAttempt();

    std::optional<EditorProject> _project;
    std::optional<WorkerSnapshots> _snapshots;
    std::unique_ptr<WorkerProcess> _active;
    std::optional<Replacement> _replacement;
    std::vector<std::unique_ptr<WorkerProcess>> _retired;
    WorkerBuild _build;
    std::optional<EditorGameBuild> _gameBuild;
    RefreshState _refresh;

    std::vector<SceneDefinition> _catalog;
    AudioUsage _audioUsage; // From the last worker that reported its catalog
    std::map<std::string, SceneParameters> _accepted;
    SceneParameters _displayed;
    SceneInspection _inspection;
    Vec2 _canvas{1280, 720};
    std::map<std::uint64_t, std::string> _pendingParameters;
    std::map<std::string, std::uint64_t> _lastParameterRequest;
    std::string _selected;
    bool _sourcesBusy = false;
    bool _gotCatalog = false;
    bool _ready = false;
    bool _paused = false;
    double _playbackSpeed = 1.0;
    std::uint64_t _generation = 0;  // The active worker's; frames and replies of other generations are stale
    std::uint64_t _generations = 0; // Last generation handed out
    std::uint64_t _request = 0;

    EditorDiagnostics _output;
    std::optional<EditorBuildResult> _attemptFailure;
    std::optional<std::string> _openedScene;
    bool _recordOpen = false;
    std::shared_ptr<const ViewportPixels> _nextFrame;
    std::shared_ptr<const ViewportPixels> _latestFrame;
    std::uint64_t _syncRequest = 0;
    EditorSync _sync;
    double _seconds = 0.0;
    nlohmann::json _simulationState;
    std::string _notice;
    std::deque<EditorControlEvent> _controlEvents;
    std::size_t _controlEventCount = 0; // Every entry this session, also those that left the deque
    std::string _controlSource;
    std::uint64_t _controlAttempt = 0;
    std::chrono::steady_clock::time_point _noticeAt;
    std::string _status = "No project";
    EditorPhase _restPhase = EditorPhase::Stopped; // Stopped or Error: the phase while nothing runs
    double _captureMilliseconds = 0.0;
    double _publishMilliseconds = 0.0;
    double _streamFps = 0.0;
    ProfilerSession _profiler;
    std::uint64_t _profilerRequest = 0;
    bool _benchmarkCancelPending = false;
    double _hostTransferMs = 0;
    TuningWorkspace _tuning;                     // Worker state is the active worker's; reset when the worker changes
    std::unique_ptr<TuningBaselines> _baselines; // Committed values; none without a project folder
    std::filesystem::path _inspectedTuning;
    std::optional<std::pair<std::filesystem::path, TuningLocation>> _previewing; // A dragged value not yet written
};
