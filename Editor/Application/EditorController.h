#pragma once
#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Platform/Windows/BuildLease.h"
#include "Editor/Internals/Platform/Windows/ChildProcess.h"
#include "Editor/Internals/Platform/Windows/ControlChannel.h"
#include "Editor/Internals/Platform/Windows/SharedFrames.h"
#include "Editor/Internals/Session/EditorProject.h"
#include "Editor/Internals/Session/RefreshState.h"

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
    void HostTransfer(double ms) { _hostTransferMs = ms; }
    void Log(const std::string& text, EditorLogLevel level = EditorLogLevel::Info,
             EditorLogSource source = EditorLogSource::Editor);
    void Execute(EditorCommand command);
    void ClearOutput() { _output.Clear(); }
    void AcknowledgeOutput() { _output.Acknowledge(); }
    [[nodiscard]] EditorSnapshot Snapshot() const;
    [[nodiscard]] std::string ProjectKey() const;
    [[nodiscard]] std::optional<std::string> TakeOpenedPreview();
    void Shutdown();
    [[nodiscard]] std::uint64_t Generation() const { return _generation; }
    [[nodiscard]] std::optional<PreviewPixels> TakeFrame();

private:
    enum class AfterStop { None, Open, Build };
    void Open();
    void Stop();
    void Pause();
    void Reset();
    void Refresh();
    void Cancel();
    [[nodiscard]] const PreviewDefinition* Selected() const;
    [[nodiscard]] bool CanOpen() const { return Snapshot().capabilities.Can(EditorCommand::Open); }
    [[nodiscard]] bool Busy() const {
        return _profiler.Snapshot().busy || _buildActive || _refresh.Busy() || _stopping ||
               (_workerActive && !_ready && !_catalogOnly);
    }
    void LaunchWorker(bool catalogOnly);
    void RequestStop(AfterStop after);
    void StartBuild();
    void Receive(const PreviewMessage& message);
    void WorkerExited(unsigned long code);
    void Fail(const std::string& text);
    void Send(const std::string& type, nlohmann::json data = nlohmann::json::object());
    void Invalidate();
    std::optional<EditorProject> _project;
    ChildProcess _worker;
    ChildProcess _build;
    std::unique_ptr<BuildLease> _buildLease;
    std::unique_ptr<ControlChannel> _channel;
    std::unique_ptr<SharedFrames> _frames;
    std::vector<PreviewDefinition> _catalog;
    std::map<std::string, PreviewParameters> _accepted;
    PreviewParameters _displayed;
    PreviewInspection _inspection;
    Vec2 _canvas{1280, 720};
    std::map<std::uint64_t, std::string> _pendingParameters;
    std::map<std::string, std::uint64_t> _lastParameterRequest;
    EditorDiagnostics _output;
    std::optional<std::string> _openedPreview;
    bool _recordOpen = false;
    std::optional<PreviewPixels> _nextFrame;
    std::string _selected;
    std::string _status = "No project";
    EditorPhase _phase = EditorPhase::Stopped;
    RefreshState _refresh;
    AfterStop _afterStop = AfterStop::None;
    std::chrono::steady_clock::time_point _deadline;
    std::optional<std::chrono::steady_clock::time_point> _connectionLostAt;
    double _captureMilliseconds = 0.0;
    double _publishMilliseconds = 0.0;
    double _streamFps = 0.0;
    std::uint64_t _generation = 0;
    std::uint64_t _request = 0;
    bool _workerActive = false;
    bool _buildActive = false;
    bool _catalogOnly = false;
    bool _gotCatalog = false;
    bool _stopping = false;
    bool _forced = false;
    bool _ready = false;
    bool _paused = false;
    bool _failed = false;
    bool _cancelBuild = false;
    double _playbackSpeed = 1.0;
    ProfilerSession _profiler;
    std::uint64_t _profilerRequest = 0;
    bool _benchmarkCancelPending = false;
    double _hostTransferMs = 0;
};
