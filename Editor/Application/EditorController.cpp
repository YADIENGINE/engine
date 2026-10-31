#include "Editor/Application/EditorController.h"
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <utility>
#include "Editor/Application/EditorCommandState.h"
#include "Editor/Internals/Platform/Windows/WindowsText.h"

namespace {
constexpr auto ShutdownWait = std::chrono::seconds(2); // On editor exit, before remaining workers are terminated
constexpr auto ShutdownPoll = std::chrono::milliseconds(2);
} // namespace

EditorController::EditorController(const std::filesystem::path& projectFile) {
    try {
        _project = ReadEditorProject(projectFile);
        _snapshots.emplace(_project->worker);
        _output.StartArchive(_project->buildDirectory / "editor-logs" / (NewPreviewSession() + ".log"));
        _tuning.Open(_project->buildDirectory);
        Log("Project: " + _project->name);
        if (!_output.ArchiveError().empty()) Log(_output.ArchiveError(), EditorLogLevel::Warning);
        if (!_snapshots->BuildAvailable()) {
            _status = "Preview executable missing";
            _restPhase = EditorPhase::Error;
            Log(_status, EditorLogLevel::Error);
            Log("Use Build & Refresh to build the configured preview target.");
            return;
        }
        _snapshots->Capture();
        _snapshots->Prune();
        Launch(PreviewWorkerMode::Catalog);
    } catch (const std::exception& error) {
        FailPreview(error.what());
    }
}
EditorController::~EditorController() { Shutdown(); }

// --- Commands --------------------------------------------------------------------------------------------------------

void EditorController::Execute(EditorCommand command) {
    if (!Snapshot().capabilities.Can(command)) return;
    switch (command) {
    case EditorCommand::Open   : Open(); break;
    case EditorCommand::Stop   : Stop(); break;
    case EditorCommand::Pause  : Pause(); break;
    case EditorCommand::Reset  : Reset(); break;
    case EditorCommand::Refresh: Refresh(); break;
    case EditorCommand::Cancel : Cancel(); break;
    case EditorCommand::Step   : Playback(_playbackSpeed, true); break;
    case EditorCommand::Count  : break;
    }
}
void EditorController::Open() {
    try {
        Launch(PreviewWorkerMode::Preview);
    } catch (const std::exception& error) {
        FailPreview(error.what());
    }
}
void EditorController::Stop() {
    if (_replacement) AbandonReplacement("the preview was stopped", EditorBuildResult::Canceled);
    if (_active)
        StopActive("Stopped");
    else if (_build.Active())
        Cancel();
}
void EditorController::Pause() {
    if (!_ready) return;
    try {
        Send("pause", {{"paused", !_paused}});
    } catch (const std::exception& error) {
        FailPreview(error.what());
    }
}
void EditorController::Reset() {
    if (!_ready) return;
    try {
        Invalidate(NextGeneration());
        _status = "Resetting preview";
        _recordOpen = false;
        _active->AwaitReady();
        Send("reset");
    } catch (const std::exception& error) {
        FailPreview(error.what());
    }
}
void EditorController::Refresh() {
    _output.BeginAttempt();
    try {
        _refresh.Begin();
        _build.Start(_project->build, _project->buildDirectory);
        _status = _ready ? "Building; the preview stays live until the new build is ready" : "Building preview target";
        Log("Build started.");
    } catch (const std::exception& error) {
        _refresh.Cancel();
        Log(error.what(), EditorLogLevel::Error);
        _output.FinishAttempt(EditorBuildResult::BuildFailed);
    }
}
void EditorController::Cancel() {
    if (_profiler.Snapshot().busy) {
        Profile(EditorProfilerCommand::Cancel, {}, {});
        return;
    }
    if (_build.Active()) {
        _build.Cancel();
        _status = "Canceling build";
    } else if (_replacement)
        AbandonReplacement("canceled", EditorBuildResult::Canceled);
    else if (_active && !_ready)
        StopActive("Canceled");
}
void EditorController::Select(const std::string& id) {
    if (Busy() || id == _selected || std::ranges::find(_catalog, id, &PreviewDefinition::id) == _catalog.end()) return;
    if (_active && _active->Mode() == PreviewWorkerMode::Preview) StopActive("Stopped");
    _selected = id;
    _displayed = _accepted[id];
}
void EditorController::Parameter(const std::string& id, const PreviewValue& value) {
    if (Busy()) return;
    try {
        const auto* selected = Selected();
        if (!selected) return;
        const auto definition = std::ranges::find(selected->parameters, id, &PreviewParameter::id);
        if (definition == selected->parameters.end()) return;
        if (auto error = ValidatePreviewValue(*definition, value); !error.empty()) {
            Log(error, EditorLogLevel::Warning);
            return;
        }
        if (!_ready) {
            _accepted[_selected][id] = value;
            _displayed[id] = value;
            return;
        }
        Send("parameter", {{"id", id}, {"value", EncodePreviewValue(value)}});
        _pendingParameters[_request] = id;
        _lastParameterRequest[id] = _request;
        _displayed[id] = value;
    } catch (const std::exception& error) {
        FailPreview(error.what());
    }
}
void EditorController::Action(const std::string& id) {
    if (_profiler.Snapshot().busy || !_ready) return;
    try {
        Send("action", {{"id", id}});
    } catch (const std::exception& error) {
        FailPreview(error.what());
    }
}
void EditorController::Tune(std::size_t file, const TuningLocation& at, const TuningValue& value) {
    if (const auto error = _tuning.Set(file, at, value); !error.empty())
        Log("Tuning not written: " + error, EditorLogLevel::Warning);
}
void EditorController::ReceiveTuning(const PreviewTuningStatus& status) {
    const std::string label = _tuning.Report(status);
    const std::string where = label.empty() ? status.source : label;
    switch (status.state) {
    case PreviewTuningState::Applied:
        if (status.reload) Log("Tuning applied: " + where, EditorLogLevel::Info, EditorLogSource::Worker);
        break;
    case PreviewTuningState::Invalid:
        Log(where + ":" + std::to_string(status.line) + ": tuning not applied: " + status.text, EditorLogLevel::Warning,
            EditorLogSource::Worker);
        break;
    case PreviewTuningState::NeedsBuild:
        Log(where + ": tuning not applied: " + status.text, EditorLogLevel::Warning, EditorLogSource::Worker);
        break;
    case PreviewTuningState::Unreadable:
        Log(where + ": tuning file unreadable: " + status.text, EditorLogLevel::Warning, EditorLogSource::Worker);
        break;
    }
}
void EditorController::Playback(double speed, bool step) {
    const auto capabilities = Snapshot().capabilities;
    if (!capabilities.playbackEnabled || (step && !capabilities.Can(EditorCommand::Step)) || !ValidPlaybackSpeed(speed))
        return;
    try {
        Send("playback", {{"speed", speed}, {"step", step}});
    } catch (const std::exception& error) {
        FailPreview(error.what());
    }
}
void EditorController::Input(const DeviceInput& input) {
    if (_profiler.Snapshot().busy || !_ready) return;
    try {
        Send("input", EncodePreviewInput(input));
    } catch (const std::exception& error) {
        FailPreview(error.what());
    }
}

// --- Output and presentation ---------------------------------------------------------------------------------------

void EditorController::Log(const std::string& text, EditorLogLevel level, EditorLogSource source) {
    _output.Append(text, level, source);
}
void EditorController::OpenOutputLog(bool attempt) {
    const auto& file = _output.ArchivePath(attempt);
    if (file.empty()) return;
    const auto result =
        reinterpret_cast<std::intptr_t>(ShellExecuteW(nullptr, L"open", file.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32)
        Log("Could not open output log (Windows shell code " + std::to_string(result) + ").", EditorLogLevel::Error);
}
EditorSnapshot EditorController::Snapshot() const {
    std::set<std::string> pending;
    for (const auto& [request, id] : _pendingParameters) {
        (void)request;
        pending.insert(id);
    }
    const auto phase = Phase();
    const bool profilerBusy = _profiler.Snapshot().busy;
    const bool benchmarkCancelPending = _benchmarkCancelPending && profilerBusy;
    EditorCommandContext context;
    context.hasProject = _project.has_value();
    context.hasBuild = _snapshots && _snapshots->Current();
    context.hasSelection = Selected() != nullptr;
    context.workerActive = _active != nullptr;
    context.starting = Starting();
    context.building = _refresh.Busy();
    context.replacing = _replacement.has_value();
    context.ready = _ready;
    context.paused = _paused;
    context.profilerBusy = profilerBusy;
    context.phase = phase;
    context.benchmarkCancelPending = benchmarkCancelPending;
    return {_catalog,
            Selected(),
            _displayed,
            std::move(pending),
            _output,
            ResolveEditorCapabilities(context),
            phase,
            _status,
            _ready,
            _ready && _paused,
            Busy(),
            _refresh.Busy(),
            _canvas,
            _generation,
            _streamFps,
            _captureMilliseconds,
            _publishMilliseconds,
            _inspection,
            _playbackSpeed,
            _profiler.Snapshot(),
            _hostTransferMs,
            benchmarkCancelPending,
            _project ? &_tuning : nullptr};
}
std::string EditorController::ProjectKey() const {
    if (!_project) return "unconfigured";
    return EditorSearchText(
               PreviewUtf8(std::filesystem::weakly_canonical(_project->buildDirectory).generic_wstring())) +
           "|" + _project->name;
}
std::optional<std::string> EditorController::TakeOpenedPreview() { return std::exchange(_openedPreview, std::nullopt); }
std::optional<PreviewPixels> EditorController::TakeFrame() { return std::exchange(_nextFrame, std::nullopt); }
const PreviewDefinition* EditorController::Selected() const {
    const auto found = std::ranges::find(_catalog, _selected, &PreviewDefinition::id);
    return found == _catalog.end() ? nullptr : &*found;
}
EditorPhase EditorController::Phase() const {
    if (_ready) return _paused ? EditorPhase::Paused : EditorPhase::Ready;
    if (_active)
        return _active->Mode() == PreviewWorkerMode::Catalog ? EditorPhase::ReadingCatalog : EditorPhase::Starting;
    if (_replacement || _build.Active()) return EditorPhase::Building;
    return _restPhase;
}
bool EditorController::Starting() const { return _active && _active->Mode() == PreviewWorkerMode::Preview && !_ready; }
bool EditorController::Busy() const { return _profiler.Snapshot().busy || _replacement || Starting(); }
PreviewOpenState EditorController::OpenState() const {
    const auto parameters = _accepted.find(_selected);
    return {_selected, parameters == _accepted.end() ? PreviewParameters{} : parameters->second, _paused,
            _playbackSpeed};
}
std::map<std::string, PreviewParameters>
EditorController::RestoreParameters(const std::vector<PreviewDefinition>& catalog) {
    auto restored = _accepted;
    for (const auto& definition : catalog) {
        std::vector<std::string> messages;
        restored[definition.id] = RestorePreviewParameters(definition, restored[definition.id], messages);
        for (const auto& text : messages)
            Log(text, EditorLogLevel::Warning);
    }
    return restored;
}

// --- Active worker -------------------------------------------------------------------------------------------------

void EditorController::Launch(PreviewWorkerMode mode) {
    if (!_snapshots || !_snapshots->Current())
        throw std::runtime_error("Nothing built to start; use Build & Refresh to build the preview target");
    if (_active && _active->Mode() == PreviewWorkerMode::Catalog) EndCatalogRefresh(EditorBuildResult::Canceled);
    _profiler.Disconnected();
    Retire(std::move(_active));
    Invalidate(NextGeneration());
    _tuning.ClearWorkerStatus();
    _active = std::make_unique<PreviewWorker>(*_snapshots->Current(), mode);
    _gotCatalog = false;
    _restPhase = EditorPhase::Stopped;
    _status = mode == PreviewWorkerMode::Catalog ? "Reading catalog" : "Starting preview";
    _recordOpen = mode == PreviewWorkerMode::Preview;
}
void EditorController::Retire(std::unique_ptr<PreviewWorker> worker) {
    if (!worker) return;
    worker->Shutdown();
    _retired.push_back(std::move(worker));
}
void EditorController::StopActive(const std::string& status) {
    if (_active && _active->Mode() == PreviewWorkerMode::Catalog) EndCatalogRefresh(EditorBuildResult::Canceled);
    _profiler.Disconnected();
    Retire(std::move(_active));
    Invalidate(NextGeneration());
    _tuning.ClearWorkerStatus();
    _status = status;
    _restPhase = EditorPhase::Stopped;
}
void EditorController::Invalidate(std::uint64_t generation) {
    _generation = generation;
    _inspection = {};
    _ready = false;
    _paused = false;
    _playbackSpeed = 1.0;
    _profiler.Reset(_generation);
    _benchmarkCancelPending = false;
    _nextFrame.reset();
    _captureMilliseconds = 0.0;
    _publishMilliseconds = 0.0;
    _streamFps = 0.0;
    _pendingParameters.clear();
    _lastParameterRequest.clear();
    _displayed = _accepted[_selected];
}
void EditorController::Send(const std::string& type, nlohmann::json data) {
    if (!_active) throw std::runtime_error("No preview connection");
    _active->Send(_generation, ++_request, type, std::move(data));
}
void EditorController::TickActive() {
    if (!_active) return;
    for (const auto& line : _active->DrainOutput())
        Log(line, EditorLogLevel::Info, EditorLogSource::Worker);
    for (const auto& message : _active->Poll())
        Receive(message);
    if (_active->ConnectionLost()) throw std::runtime_error("Preview control connection lost");
    if (auto frame = _active->ReadFrame(_generation)) _nextFrame = std::move(frame);
    if (const auto code = _active->ExitCode()) {
        const auto mode = _active->Mode();
        for (const auto& line : _active->DrainOutput())
            Log(line, EditorLogLevel::Info, EditorLogSource::Worker);
        _active.reset();
        ActiveExited(mode, *code);
    } else if (_active->StartupTimedOut())
        throw std::runtime_error("Preview startup/first-frame timeout");
}
void EditorController::Receive(const PreviewMessage& message) {
    if (message.type == "fatal") throw std::runtime_error(message.data.at("text").get<std::string>());
    if (message.type == "log") {
        Log(message.data.at("text").get<std::string>(), EditorLogLevel::Info, EditorLogSource::Worker);
        return;
    }
    if (message.type == "tuning") {
        ReceiveTuning(DecodePreviewTuning(message.data));
        return;
    }
    if (message.type == "catalog") {
        _catalog = DecodePreviewCatalog(message.data);
        _accepted = RestoreParameters(_catalog);
        _gotCatalog = true;
        _displayed = _accepted[_selected];
        if (_active->Mode() == PreviewWorkerMode::Catalog)
            Send("catalog-ack");
        else if (Selected())
            Send("open", EncodePreviewOpen(OpenState()));
        else
            throw std::runtime_error("Selected content is absent from the preview catalog");
        return;
    }
    if (message.generation != _generation) return;
    if (message.type.starts_with("profiler-")) {
        _profiler.Receive(message);
    } else if (message.type == "ready") {
        ApplyReady(message);
    } else if (message.type == "parameters") {
        const auto values = DecodePreviewParameters(message.data.at("parameters"));
        const auto* definition = Selected();
        if (!definition) return;
        (void)MergePreviewCapture(*definition, values, _accepted[_selected]);
        for (const auto& [id, value] : _accepted[_selected]) {
            const bool pending =
                std::ranges::any_of(_pendingParameters, [&](const auto& entry) { return entry.second == id; });
            if (!pending) _displayed[id] = value;
        }
    } else if (message.type == "inspection") {
        _inspection = DecodePreviewInspection(message.data);
    } else if (message.type == "metrics") {
        const double capture = message.data.at("captureMs").get<double>();
        const double publish = message.data.at("publishMs").get<double>();
        const double fps = message.data.at("fps").get<double>();
        if (!std::isfinite(capture) || !std::isfinite(publish) || !std::isfinite(fps) || capture < 0.0 ||
            publish < 0.0 || fps < 0.0)
            throw std::runtime_error("Invalid preview timing metrics");
        _captureMilliseconds = capture;
        _publishMilliseconds = publish;
        _streamFps = fps;
    } else if (message.type == "state") {
        _paused = message.data.at("paused").get<bool>();
        _playbackSpeed = message.data.value("speed", _playbackSpeed);
    } else if (message.type == "accepted" || message.type == "rejected") {
        const auto pending = _pendingParameters.find(message.request);
        if (pending != _pendingParameters.end()) {
            const auto id = pending->second;
            if (message.type == "accepted") _accepted[_selected][id] = DecodePreviewValue(message.data.at("value"));
            if (_lastParameterRequest[id] == message.request) _displayed[id] = _accepted[_selected].at(id);
            _pendingParameters.erase(pending);
        }
        if (message.type == "rejected")
            Log(message.data.at("text").get<std::string>(), EditorLogLevel::Warning, EditorLogSource::Worker);
    }
}
void EditorController::ApplyReady(const PreviewMessage& message) {
    const auto& canvas = message.data.at("canvas");
    _canvas = {canvas.at(0).get<int>(), canvas.at(1).get<int>()};
    if (_canvas.x <= 0 || _canvas.y <= 0 || _canvas.x > PreviewMaxFrameWidth || _canvas.y > PreviewMaxFrameHeight)
        throw std::runtime_error("Invalid logical canvas dimensions");
    _accepted[_selected] = DecodePreviewParameters(message.data.at("parameters"));
    _displayed = _accepted[_selected];
    _paused = message.data.at("paused").get<bool>();
    _playbackSpeed = message.data.at("speed").get<double>();
    _ready = true;
    _active->MarkReady();
    if (_profiler.Snapshot().liveEnabled) Send("profiler", {{"operation", "enable"}, {"enabled", true}});
    _status = "Preview ready";
    if (_recordOpen) _openedPreview = _selected;
    _recordOpen = false;
}
void EditorController::ActiveExited(PreviewWorkerMode mode, unsigned long code) {
    _profiler.Disconnected();
    Invalidate(NextGeneration());
    _restPhase = EditorPhase::Stopped;
    if (mode == PreviewWorkerMode::Catalog && code == 0 && _gotCatalog) {
        _status = "Catalog ready; preview stopped";
        EndCatalogRefresh(EditorBuildResult::Succeeded);
        return;
    }
    if (mode == PreviewWorkerMode::Preview && code == 0) {
        _status = "Preview closed normally";
        Log(_status);
        return;
    }
    throw std::runtime_error("Preview process failed (exit " + std::to_string(code) + ")");
}

// --- Build & Refresh -----------------------------------------------------------------------------------------------

void EditorController::TickBuild() {
    if (!_build.Active()) return;
    for (const auto& line : _build.DrainOutput())
        Log(line, ClassifyBuildOutput(line), EditorLogSource::Build);
    if (const auto exit = _build.Finish()) BuildFinished(*exit);
}
void EditorController::BuildFinished(const PreviewBuildExit& exit) {
    for (const auto& line : exit.output)
        Log(line, ClassifyBuildOutput(line), EditorLogSource::Build);
    _output.BuildExited(exit.code);
    switch (exit.outcome) {
    case PreviewBuildOutcome::Aborted: return; // The failure that aborted it settles the attempt
    case PreviewBuildOutcome::Canceled:
        _refresh.Cancel();
        _status = "Build canceled";
        Log(_status);
        _output.FinishAttempt(EditorBuildResult::Canceled);
        return;
    case PreviewBuildOutcome::Failed:
        _refresh.BuildFinished(false);
        Log("Build failed (exit " + std::to_string(exit.code) + ").", EditorLogLevel::Error, EditorLogSource::Build);
        _status = _active ? "Build failed; the preview still runs the previous build" : "Build failed";
        if (!_active) _restPhase = EditorPhase::Error;
        _output.FinishAttempt(EditorBuildResult::BuildFailed);
        return;
    case PreviewBuildOutcome::Succeeded:
        _refresh.BuildFinished(true);
        Log("Build succeeded.");
        _snapshots->Capture();
        if (_active && _active->Mode() == PreviewWorkerMode::Preview)
            StartReplacement();
        else
            Launch(PreviewWorkerMode::Catalog); // No preview is open: refresh only the catalog
        return;
    }
}
void EditorController::StartReplacement() {
    auto worker = std::make_unique<PreviewWorker>(*_snapshots->Current(), PreviewWorkerMode::Preview);
    _replacement.emplace();
    _replacement->worker = std::move(worker);
    _replacement->generation = NextGeneration();
    _replacement->state = OpenState();
    _status = "Starting the new build beside the running preview";
}
void EditorController::EndCatalogRefresh(EditorBuildResult result) {
    if (_refresh.Phase() != RefreshPhase::Starting || _replacement) return;
    if (result == EditorBuildResult::Succeeded)
        _refresh.Started();
    else
        _refresh.Cancel();
    _output.FinishAttempt(result);
}
void EditorController::TickReplacement() {
    if (!_replacement) return;
    try {
        auto& worker = *_replacement->worker;
        for (const auto& line : worker.DrainOutput())
            Log(line, EditorLogLevel::Info, EditorLogSource::Worker);
        for (const auto& message : worker.Poll()) {
            ReceiveReplacement(message);
            if (!_replacement) return;
        }
        if (const auto code = worker.ExitCode()) {
            for (const auto& line : worker.DrainOutput())
                Log(line, EditorLogLevel::Info, EditorLogSource::Worker);
            if (*code == 0 && _replacement->selectionMissing) {
                CommitCatalogOnly();
                return;
            }
            throw std::runtime_error("it exited before it was ready (exit " + std::to_string(*code) + ")");
        }
        if (worker.StartupTimedOut()) throw std::runtime_error("it did not become ready in time");
        if (worker.ConnectionLost()) throw std::runtime_error("its control connection was lost");
    } catch (const std::exception& error) {
        AbandonReplacement(error.what(), EditorBuildResult::RefreshFailed);
    }
}
void EditorController::ReceiveReplacement(const PreviewMessage& message) {
    auto& replacement = *_replacement;
    if (message.type == "fatal") throw std::runtime_error(message.data.at("text").get<std::string>());
    if (message.type == "log") {
        Log(message.data.at("text").get<std::string>(), EditorLogLevel::Info, EditorLogSource::Worker);
    } else if (message.type == "tuning") {
        replacement.tuning.push_back(DecodePreviewTuning(message.data));
    } else if (message.type == "catalog") {
        replacement.catalog = DecodePreviewCatalog(message.data);
        replacement.accepted = RestoreParameters(*replacement.catalog);
        if (std::ranges::find(*replacement.catalog, replacement.state.id, &PreviewDefinition::id) ==
            replacement.catalog->end()) {
            replacement.selectionMissing = true;
            replacement.worker->Shutdown(); // It exits normally; the catalog alone is taken over
            return;
        }
        replacement.state.parameters = replacement.accepted[replacement.state.id];
        replacement.worker->Send(replacement.generation, ++_request, "open", EncodePreviewOpen(replacement.state));
    } else if (message.type == "ready" && message.generation == replacement.generation) {
        CommitReplacement(message);
    }
}
void EditorController::CommitReplacement(const PreviewMessage& ready) {
    auto replacement = std::move(*_replacement);
    _replacement.reset();
    _profiler.Disconnected();
    Retire(std::move(_active));
    _active = std::move(replacement.worker);
    _catalog = std::move(*replacement.catalog);
    _accepted = std::move(replacement.accepted);
    _gotCatalog = true;
    Invalidate(replacement.generation);
    _tuning.ClearWorkerStatus();
    for (const auto& status : replacement.tuning)
        ReceiveTuning(status);
    _recordOpen = false;
    ApplyReady(ready);
    _refresh.Started();
    Log("The new build replaced the preview.");
    _output.FinishAttempt(EditorBuildResult::Succeeded);
    _snapshots->Prune();
}
void EditorController::CommitCatalogOnly() {
    auto replacement = std::move(*_replacement);
    _replacement.reset();
    StopActive("The new build has no '" + _selected + "' preview; preview stopped");
    _catalog = std::move(*replacement.catalog);
    _accepted = std::move(replacement.accepted);
    _gotCatalog = true;
    _displayed = _accepted[_selected];
    Log(_status, EditorLogLevel::Warning);
    _refresh.Started();
    _output.FinishAttempt(EditorBuildResult::Succeeded);
    _snapshots->Prune();
}
void EditorController::AbandonReplacement(const std::string& reason, EditorBuildResult result) {
    auto replacement = std::move(*_replacement);
    _replacement.reset();
    if (replacement.worker) {
        replacement.worker->Terminate();
        Retire(std::move(replacement.worker));
    }
    _refresh.Cancel();
    const auto text = "The new build did not replace the preview: " + reason;
    Log(text, result == EditorBuildResult::Canceled ? EditorLogLevel::Info : EditorLogLevel::Error);
    _status = _active ? std::string("The preview still runs the previous build") : text;
    _output.FinishAttempt(result);
}

// --- Retired workers, failures and shutdown ------------------------------------------------------------------------

void EditorController::TickRetired() {
    bool exited = false;
    for (auto worker = _retired.begin(); worker != _retired.end();) {
        try {
            for (const auto& message : (*worker)->Poll())
                if (message.type == "log")
                    Log(message.data.at("text").get<std::string>(), EditorLogLevel::Info, EditorLogSource::Worker);
            for (const auto& line : (*worker)->DrainOutput())
                Log(line, EditorLogLevel::Info, EditorLogSource::Worker);
            if ((*worker)->EnforceShutdown())
                Log("A stopped preview did not exit in time; terminating its process tree.", EditorLogLevel::Warning);
        } catch (const std::exception& error) {
            Log(std::string("Stopped preview: ") + error.what(), EditorLogLevel::Warning);
            (*worker)->Terminate();
        }
        if (const auto code = (*worker)->ExitCode()) {
            for (const auto& line : (*worker)->DrainOutput())
                Log(line, EditorLogLevel::Info, EditorLogSource::Worker);
            Log("Preview process exited (" + std::to_string(*code) + ").");
            worker = _retired.erase(worker);
            exited = true;
        } else
            ++worker;
    }
    if (exited && _snapshots) _snapshots->Prune(); // The snapshot it ran from is free now
}
void EditorController::FailPreview(const std::string& text) {
    Log(text, EditorLogLevel::Error);
    _status = text;
    _restPhase = EditorPhase::Error;
    _profiler.Disconnected();
    if (_active) {
        _active->Terminate();
        Retire(std::move(_active));
    }
    Invalidate(NextGeneration());
    EndCatalogRefresh(EditorBuildResult::RefreshFailed);
}
void EditorController::FailRefresh(const std::string& text) {
    Log(text, EditorLogLevel::Error);
    _status = text;
    const auto& attempt = _output.Attempt();
    if (attempt.result == EditorBuildResult::Running)
        _attemptFailure = attempt.exitCode && *attempt.exitCode == 0 ? EditorBuildResult::RefreshFailed
                                                                     : EditorBuildResult::BuildFailed;
    _build.Abort();
    if (_replacement) {
        _replacement->worker->Terminate();
        Retire(std::move(_replacement->worker));
        _replacement.reset();
    }
    _refresh.Cancel();
}
void EditorController::FinishFailedAttempt() {
    if (!_attemptFailure || _build.Active()) return;
    _output.FinishAttempt(*_attemptFailure);
    _attemptFailure.reset();
}
void EditorController::Tick() {
    _tuning.Poll();
    try {
        TickBuild();
    } catch (const std::exception& error) {
        FailRefresh(error.what());
    }
    try {
        TickActive();
    } catch (const std::exception& error) {
        FailPreview(error.what());
    }
    TickReplacement();
    TickRetired();
    FinishFailedAttempt();
}
void EditorController::Shutdown() {
    if (_replacement) {
        Retire(std::move(_replacement->worker));
        _replacement.reset();
    }
    Retire(std::move(_active));
    _build.Abort();
    const auto deadline = std::chrono::steady_clock::now() + ShutdownWait;
    while (std::ranges::any_of(_retired, [](const auto& worker) { return worker->Running(); }) &&
           std::chrono::steady_clock::now() < deadline) {
        for (const auto& worker : _retired) {
            try {
                (void)worker->Poll();
                (void)worker->DrainOutput();
            } catch (...) {
                worker->Terminate();
            }
        }
        std::this_thread::sleep_for(ShutdownPoll);
    }
    _retired.clear(); // Destructors terminate what still runs
}
