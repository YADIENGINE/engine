#include "Editor/Application/EditorController.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <utility>
#include <sstream>
#include "Editor/Application/EditorCommandState.h"
#include "Platform/UserData.h"
#include "Platform/FileStorage.h"
#include "Editor/Internals/Platform/Windows/WindowsText.h"

EditorController::EditorController(const std::filesystem::path& projectFile) {
    try {
        _project = ReadEditorProject(projectFile);
        Log("Project: " + _project->name);
        if (std::filesystem::is_regular_file(_project->worker.executable))
            LaunchWorker(true);
        else {
            _status = "Preview executable missing";
            _phase = EditorPhase::Error;
            Log(_status, EditorLogLevel::Error);
            Log("Use Build & Refresh to build the configured preview target.");
        }
    } catch (const std::exception& error) {
        Fail(error.what());
    }
}
EditorController::~EditorController() { Shutdown(); }
void EditorController::Playback(double speed, bool step) {
    const auto capabilities = Snapshot().capabilities;
    if (!capabilities.playbackEnabled || (step && !capabilities.Can(EditorCommand::Step)) || !std::isfinite(speed) ||
        speed < 0.1 || speed > 4.0)
        return;
    Send("playback", {{"speed", speed}, {"step", step}});
}
void EditorController::Log(const std::string& text, EditorLogLevel level, EditorLogSource source) {
    _output.Append(text, level, source);
}
EditorSnapshot EditorController::Snapshot() const {
    std::set<std::string> pending;
    for (const auto& [request, id] : _pendingParameters) {
        (void)request;
        pending.insert(id);
    }
    const auto phase = _ready ? (_paused ? EditorPhase::Paused : EditorPhase::Ready) : _phase;
    const bool benchmarkCancelPending = _benchmarkCancelPending && _profiler.Snapshot().busy;
    const auto capabilities = ResolveEditorCapabilities({_project.has_value(), Selected() != nullptr, _workerActive,
                                                         _buildActive, _refresh.Busy(), _stopping, _ready, _paused,
                                                         _profiler.Snapshot().busy, phase, benchmarkCancelPending});
    return {_catalog,
            Selected(),
            _displayed,
            std::move(pending),
            _output,
            capabilities,
            phase,
            _status,
            _ready,
            _ready && _paused,
            Busy(),
            _canvas,
            _generation,
            _streamFps,
            _captureMilliseconds,
            _publishMilliseconds,
            _inspection,
            _playbackSpeed,
            _profiler.Snapshot(),
            _hostTransferMs,
            benchmarkCancelPending};
}
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
std::string EditorController::ProjectKey() const {
    if (!_project) return "unconfigured";
    return EditorSearchText(
               PreviewUtf8(std::filesystem::weakly_canonical(_project->buildDirectory).generic_wstring())) +
           "|" + _project->name;
}
std::optional<std::string> EditorController::TakeOpenedPreview() { return std::exchange(_openedPreview, std::nullopt); }
const PreviewDefinition* EditorController::Selected() const {
    const auto found = std::ranges::find(_catalog, _selected, &PreviewDefinition::id);
    return found == _catalog.end() ? nullptr : &*found;
}
void EditorController::Invalidate() {
    _inspection = {};
    _playbackSpeed = 1.0;
    _ready = false;
    ++_generation;
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
    if (!_channel || _channel->Closed()) throw std::runtime_error("No preview connection");
    _channel->Send({{}, _generation, ++_request, type, std::move(data)});
}
void EditorController::LaunchWorker(bool catalogOnly) {
    if (!_project) return;
    if (_worker.Running() || _build.Running()) throw std::logic_error("Cannot replace a running worker/build");
    _worker.Clear();
    Invalidate();
    _failed = false;
    _stopping = false;
    _forced = false;
    _catalogOnly = catalogOnly;
    _gotCatalog = false;
    _connectionLostAt.reset();
    const auto session = NewPreviewSession();
    _channel = std::make_unique<ControlChannel>(session, true);
    if (catalogOnly)
        _frames.reset();
    else
        _frames = std::make_unique<SharedFrames>(session, true);
    auto command = _project->worker;
    command.arguments.insert(command.arguments.end(), {"--session", session});
    if (catalogOnly) command.arguments.push_back("--catalog");
    _worker.Start(command);
    _workerActive = true;
    _deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    _status = catalogOnly ? "Reading catalog" : "Starting preview";
    _phase = catalogOnly ? EditorPhase::ReadingCatalog : EditorPhase::Starting;
    _recordOpen = !catalogOnly;
}
void EditorController::Select(const std::string& id) {
    if (Busy() || id == _selected || std::ranges::find(_catalog, id, &PreviewDefinition::id) == _catalog.end()) return;
    if (_workerActive) Stop();
    _selected = id;
    _displayed = _accepted[id];
}
void EditorController::Open() {
    if (!CanOpen()) return;
    try {
        if (_workerActive)
            RequestStop(AfterStop::Open);
        else
            LaunchWorker(false);
    } catch (const std::exception& error) {
        Fail(error.what());
    }
}
void EditorController::RequestStop(AfterStop after) {
    if (_stopping) {
        _afterStop = after;
        return;
    }
    _failed = false;
    Invalidate();
    _afterStop = after;
    _status = "Stopping preview";
    _phase = EditorPhase::Stopping;
    if (!_workerActive || !_worker.Running()) {
        _worker.Clear();
        _workerActive = false;
        _channel.reset();
        _frames.reset();
        _stopping = false;
        _afterStop = AfterStop::None;
        if (after == AfterStop::Build) {
            if (_refresh.Phase() == RefreshPhase::Stopping) _refresh.WorkerExited();
            StartBuild();
        } else if (after == AfterStop::Open)
            LaunchWorker(false);
        else {
            _status = "Stopped";
            _phase = EditorPhase::Stopped;
        }
        return;
    }
    _stopping = true;
    _forced = false;
    _deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    if (_channel && !_channel->Closed())
        Send("shutdown");
    else
        _worker.Terminate();
}
void EditorController::Stop() {
    if (_refresh.Busy()) {
        Cancel();
        return;
    }
    try {
        RequestStop(AfterStop::None);
    } catch (const std::exception& error) {
        Fail(error.what());
    }
}
void EditorController::Pause() {
    if (!_ready) return;
    try {
        Send("pause", {{"paused", !_paused}});
    } catch (const std::exception& error) {
        Fail(error.what());
    }
}
void EditorController::Reset() {
    if (!_ready) return;
    try {
        Invalidate();
        _status = "Resetting preview";
        _phase = EditorPhase::Starting;
        _recordOpen = false;
        _deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        Send("reset");
    } catch (const std::exception& error) {
        Fail(error.what());
    }
}
void EditorController::Refresh() {
    if (!_project || _refresh.Busy()) return;
    try {
        _refresh.Begin(_worker.Running());
        RequestStop(AfterStop::Build);
    } catch (const std::exception& error) {
        Fail(error.what());
    }
}
void EditorController::StartBuild() {
    if (!_project || _worker.Running() || _refresh.Phase() != RefreshPhase::Building)
        throw std::logic_error("Preview build requires confirmed worker exit");
    _buildLease = std::make_unique<BuildLease>(_project->buildDirectory);
    _cancelBuild = false;
    _build.Start(_project->build);
    _buildActive = true;
    _status = "Building preview target";
    _phase = EditorPhase::Building;
    Log("Build started after preview process exit.");
}
void EditorController::Cancel() {
    if (_profiler.Snapshot().busy) {
        Profile(EditorProfilerCommand::Cancel, {}, {});
        return;
    }
    try {
        _afterStop = AfterStop::None;
        if (_buildActive) {
            _cancelBuild = true;
            _build.Terminate();
            _status = "Canceling build";
            _phase = EditorPhase::Stopping;
        } else {
            _refresh.Cancel();
            RequestStop(AfterStop::None);
        }
    } catch (const std::exception& error) {
        Fail(error.what());
    }
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
        Fail(error.what());
    }
}
void EditorController::Action(const std::string& id) {
    if (_profiler.Snapshot().busy) return;
    if (!_ready) return;
    try {
        Send("action", {{"id", id}});
    } catch (const std::exception& error) {
        Fail(error.what());
    }
}
void EditorController::Input(const DeviceInput& input) {
    if (_profiler.Snapshot().busy) return;
    if (!_ready) return;
    try {
        Send("input", EncodePreviewInput(input));
    } catch (const std::exception& error) {
        Fail(error.what());
    }
}
void EditorController::Receive(const PreviewMessage& message) {
    if (message.type == "fatal") throw std::runtime_error(message.data.at("text").get<std::string>());
    if (message.type == "log") {
        Log(message.data.at("text").get<std::string>(), EditorLogLevel::Info, EditorLogSource::Worker);
        return;
    }
    if (message.type == "catalog") {
        _catalog = DecodePreviewCatalog(message.data);
        _gotCatalog = true;
        for (const auto& definition : _catalog) {
            std::vector<std::string> messages;
            _accepted[definition.id] = RestorePreviewParameters(definition, _accepted[definition.id], messages);
            for (const auto& text : messages)
                Log(text, EditorLogLevel::Warning);
        }
        _displayed = _accepted[_selected];
        if (_stopping) return;
        if (_catalogOnly) {
            Send("catalog-ack");
        } else {
            if (!Selected()) throw std::runtime_error("Selected content is absent from the new preview catalog");
            Send("open", {{"id", _selected}, {"parameters", EncodePreviewParameters(_accepted[_selected])}});
        }
        return;
    }
    if (message.generation != _generation || _stopping) return;
    if (message.type.starts_with("profiler-")) {
        _profiler.Receive(message);
        return;
    }
    if (message.type == "ready") {
        const auto& canvas = message.data.at("canvas");
        _canvas = {canvas.at(0).get<int>(), canvas.at(1).get<int>()};
        if (_canvas.x <= 0 || _canvas.y <= 0 || _canvas.x > PreviewMaxFrameWidth || _canvas.y > PreviewMaxFrameHeight)
            throw std::runtime_error("Invalid logical canvas dimensions");
        _accepted[_selected] = DecodePreviewParameters(message.data.at("parameters"));
        _displayed = _accepted[_selected];
        _paused = message.data.at("paused").get<bool>();
        _ready = true;
        if (_profiler.Snapshot().liveEnabled) Send("profiler", {{"operation", "enable"}, {"enabled", true}});
        _status = "Preview ready";
        _phase = EditorPhase::Ready;
        if (_recordOpen) _openedPreview = _selected;
        _recordOpen = false;
        if (_refresh.Phase() == RefreshPhase::Starting) _refresh.Started();
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
void EditorController::WorkerExited(unsigned long code) {
    _profiler.Disconnected();
    for (const auto& line : _worker.DrainOutput())
        Log(line, EditorLogLevel::Info, EditorLogSource::Worker);
    const auto after = std::exchange(_afterStop, AfterStop::None);
    const bool expected = _stopping;
    _workerActive = false;
    _stopping = false;
    _worker.Clear();
    _channel.reset();
    _frames.reset();
    Invalidate();
    if (_failed) return;
    if (expected) {
        Log("Preview process exited (" + std::to_string(code) + ").");
        _status = "Stopped";
        _phase = EditorPhase::Stopped;
        if (after == AfterStop::Build) {
            _refresh.WorkerExited();
            StartBuild();
        } else if (after == AfterStop::Open)
            LaunchWorker(false);
    } else if (_catalogOnly && code == 0 && _gotCatalog) {
        _status = "Catalog ready; preview stopped";
        _phase = EditorPhase::Stopped;
        if (_refresh.Phase() == RefreshPhase::Starting) _refresh.Started();
    } else if (code == 0 && !_catalogOnly) {
        _status = "Preview closed normally";
        _phase = EditorPhase::Stopped;
        Log(_status);
        _refresh.Cancel();
    } else {
        throw std::runtime_error("Preview process failed (exit " + std::to_string(code) + ")");
    }
}
void EditorController::Tick() {
    try {
        for (const auto& line : _worker.DrainOutput())
            Log(line, EditorLogLevel::Info, EditorLogSource::Worker);
        for (const auto& line : _build.DrainOutput())
            Log(line, ClassifyBuildOutput(line), EditorLogSource::Build);
        if (_channel) {
            for (const auto& message : _channel->Poll())
                Receive(message);
            if (_channel->Closed() && _worker.Running() && !_catalogOnly && !_stopping) {
                const auto now = std::chrono::steady_clock::now();
                if (!_connectionLostAt) _connectionLostAt = now;
                // The peer can close its handle just before a normal process exit becomes observable.
                if (now - *_connectionLostAt > std::chrono::milliseconds(250))
                    throw std::runtime_error("Preview control connection lost");
            }
        }
        if (_frames && !_stopping && !_catalogOnly) {
            if (auto frame = _frames->Read(_generation)) _nextFrame = std::move(frame);
        }
        if (_workerActive) {
            if (auto code = _worker.ExitCode())
                WorkerExited(*code);
            else if (std::chrono::steady_clock::now() > _deadline) {
                if (_stopping && !_forced) {
                    Log("Preview shutdown timed out; terminating its owned process tree.", EditorLogLevel::Warning);
                    _worker.Terminate();
                    _forced = true;
                } else if (!_stopping && !_ready)
                    throw std::runtime_error("Preview startup/first-frame timeout");
            }
        }
        if (_buildActive) {
            if (auto code = _build.ExitCode()) {
                for (const auto& line : _build.DrainOutput())
                    Log(line, ClassifyBuildOutput(line), EditorLogSource::Build);
                _buildActive = false;
                _build.Clear();
                _buildLease.reset();
                if (_cancelBuild) {
                    _refresh.Cancel();
                    if (!_failed) {
                        _status = "Build canceled";
                        _phase = EditorPhase::Stopped;
                    }
                    Log("Build canceled");
                } else {
                    _refresh.BuildFinished(*code == 0);
                    if (*code == 0) {
                        Log("Build succeeded.");
                        LaunchWorker(_selected.empty());
                    } else {
                        _status = "Build failed";
                        _phase = EditorPhase::Error;
                        Log("Build failed (exit " + std::to_string(*code) + ").", EditorLogLevel::Error,
                            EditorLogSource::Build);
                    }
                }
            }
        }
    } catch (const std::exception& error) {
        Fail(error.what());
    }
}
std::optional<PreviewPixels> EditorController::TakeFrame() { return std::exchange(_nextFrame, std::nullopt); }
void EditorController::Fail(const std::string& text) {
    Log(text, EditorLogLevel::Error);
    _status = text;
    _phase = EditorPhase::Error;
    _failed = true;
    _afterStop = AfterStop::None;
    _refresh.Cancel();
    Invalidate();
    _channel.reset();
    _frames.reset();
    _cancelBuild = true;
    try {
        _worker.Terminate();
        _build.Terminate();
    } catch (const std::exception& error) {
        Log(error.what(), EditorLogLevel::Error);
    }
    if (!_build.Running()) {
        _build.Clear();
        _buildActive = false;
        _buildLease.reset();
    }
    // Tick reaps children; no new build may pass the process-exit gate in the meantime.
}
void EditorController::Shutdown() {
    if (_worker.Running() && _channel && !_channel->Closed()) {
        try {
            Send("shutdown");
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            while (_worker.Running() && std::chrono::steady_clock::now() < deadline) {
                (void)_channel->Poll();
                (void)_worker.DrainOutput();
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        } catch (...) {
        }
    }
    _worker.Clear();
    _build.Clear();
    _buildLease.reset();
    _channel.reset();
    _frames.reset();
}
