#pragma once
#include <map>
#include <memory>
#include <optional>
#include <vector>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Application/PreviewBuild.h"
#include "Editor/Application/PreviewWorker.h"
#include "Editor/Internals/Session/EditorProject.h"
#include "Editor/Internals/Session/RefreshState.h"
#include "Editor/Internals/Session/TuningWorkspace.h"
#include "Editor/Internals/Session/WorkerSnapshots.h"

// The editor session: the catalog, the selected preview with its parameters, and the workers behind it.
// Workers run from snapshots of the build (WorkerSnapshots), so a build never waits for a worker to exit. At most one
// worker is active; Build & Refresh starts the new build as a replacement beside it and swaps once that is ready.
// Stopped and replaced workers retire: they shut down in the background while the session moves on.
class EditorController {
public:
    explicit EditorController(const std::filesystem::path& projectFile);
    ~EditorController();
    void Tick();
    void Select(const std::string& id);
    void Parameter(const std::string& id, const PreviewValue& value);
    void Action(const std::string& id);
    void Playback(double speed, bool step);
    void Input(const DeviceInput& input);
    void Profile(EditorProfilerCommand command, const Profiler::Recipe& recipe, const std::string& file);
    void Tune(std::size_t file, const TuningLocation& at, const TuningValue& value);
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
    [[nodiscard]] std::optional<std::string> TakeOpenedPreview();
    void Shutdown();
    [[nodiscard]] std::uint64_t Generation() const { return _generation; }
    [[nodiscard]] std::optional<PreviewPixels> TakeFrame();

private:
    // The worker that will replace the active one, with what it brings until the swap.
    struct Replacement {
        std::unique_ptr<PreviewWorker> worker;
        std::uint64_t generation = 0;
        PreviewOpenState state;
        std::optional<std::vector<PreviewDefinition>> catalog;
        std::map<std::string, PreviewParameters> accepted;
        bool selectionMissing = false; // The new catalog lacks the selected preview: it only refreshes the catalog
        std::vector<PreviewTuningStatus> tuning;
    };

    void Open();
    void Stop();
    void Pause();
    void Reset();
    void Refresh();
    void Cancel();
    [[nodiscard]] const PreviewDefinition* Selected() const;
    [[nodiscard]] EditorPhase Phase() const;
    [[nodiscard]] bool Starting() const;
    [[nodiscard]] bool Busy() const;
    [[nodiscard]] PreviewOpenState OpenState() const;
    [[nodiscard]] std::map<std::string, PreviewParameters>
    RestoreParameters(const std::vector<PreviewDefinition>& catalog);
    [[nodiscard]] std::uint64_t NextGeneration() { return ++_generations; }

    void Launch(PreviewWorkerMode mode);
    void Retire(std::unique_ptr<PreviewWorker> worker);
    void StopActive(const std::string& status);
    void Invalidate(std::uint64_t generation);
    void Send(const std::string& type, nlohmann::json data = nlohmann::json::object());

    void TickActive();
    void Receive(const PreviewMessage& message);
    void ApplyReady(const PreviewMessage& message);
    void ActiveExited(PreviewWorkerMode mode, unsigned long code);
    // Worker'ın tuning raporu: panelin durumu ve Output (ilk başarılı okuma hariç).
    void ReceiveTuning(const PreviewTuningStatus& status);

    void TickBuild();
    void BuildFinished(const PreviewBuildExit& exit);
    void StartReplacement();
    // With no preview open, Build & Refresh ends with the catalog worker that reads the new build.
    void EndCatalogRefresh(EditorBuildResult result);
    void TickReplacement();
    void ReceiveReplacement(const PreviewMessage& message);
    void CommitReplacement(const PreviewMessage& ready);
    void CommitCatalogOnly();
    void AbandonReplacement(const std::string& reason, EditorBuildResult result);
    void TickRetired();

    // A fault of the active worker stops only the preview; a fault of Build & Refresh leaves the preview running.
    void FailPreview(const std::string& text);
    void FailRefresh(const std::string& text);
    void FinishFailedAttempt();

    std::optional<EditorProject> _project;
    std::optional<WorkerSnapshots> _snapshots;
    std::unique_ptr<PreviewWorker> _active;
    std::optional<Replacement> _replacement;
    std::vector<std::unique_ptr<PreviewWorker>> _retired;
    PreviewBuild _build;
    RefreshState _refresh;

    std::vector<PreviewDefinition> _catalog;
    std::map<std::string, PreviewParameters> _accepted;
    PreviewParameters _displayed;
    PreviewInspection _inspection;
    Vec2 _canvas{1280, 720};
    std::map<std::uint64_t, std::string> _pendingParameters;
    std::map<std::string, std::uint64_t> _lastParameterRequest;
    std::string _selected;
    bool _gotCatalog = false;
    bool _ready = false;
    bool _paused = false;
    double _playbackSpeed = 1.0;
    std::uint64_t _generation = 0;  // The active worker's; frames and replies of other generations are stale
    std::uint64_t _generations = 0; // Last generation handed out
    std::uint64_t _request = 0;

    EditorDiagnostics _output;
    std::optional<EditorBuildResult> _attemptFailure;
    std::optional<std::string> _openedPreview;
    bool _recordOpen = false;
    std::optional<PreviewPixels> _nextFrame;
    std::string _status = "No project";
    EditorPhase _restPhase = EditorPhase::Stopped; // Stopped or Error: the phase while nothing runs
    double _captureMilliseconds = 0.0;
    double _publishMilliseconds = 0.0;
    double _streamFps = 0.0;
    ProfilerSession _profiler;
    std::uint64_t _profilerRequest = 0;
    bool _benchmarkCancelPending = false;
    double _hostTransferMs = 0;
    TuningWorkspace _tuning; // Durumu aktif worker'ın; worker değişince sıfırlanır
};
