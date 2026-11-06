#include "Editor/Worker/WorkerSession.h"
#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <utility>
#include <stdexcept>
#include "Editor/Internals/Viewport/ViewportInputRouter.h"
#include "Runtime/Game.h"
#include "Tuning/TuningLive.h"
#include "Diagnostics/Internals/CrashSession.h"

namespace {
constexpr double StepSeconds = 1.0 / 60.0; // One Step, and one seek step
constexpr double MaxSeekSeconds = 600.0;
constexpr std::uint64_t MaxSeekSteps = 600 * 240; // Ten minutes at the highest step rate
constexpr double SeekTolerance = 1e-6;
constexpr int SeekStepAllowance = 4; // Steps a seek may take per planned step before it gives up where it is
constexpr int SeekExtraSteps = 120;
struct CapturedImage {
    explicit CapturedImage(Image value)
        : image(value) {}
    Image image;
    ~CapturedImage() {
        if (image.data) UnloadImage(image);
    }
    CapturedImage(const CapturedImage&) = delete;
    CapturedImage& operator=(const CapturedImage&) = delete;
};
std::filesystem::path Utf8Path(const std::string& text) {
    return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}
} // namespace

WorkerSession::WorkerSession(Game& game, const GameSettings& settings, const SceneRegistry& registry,
                             ControlChannel& channel, const std::string& session, const FontFace* font,
                             AudioSystem& audio, SoundPlayer& sounds, WorkerSessionActions actions)
    : _audio(audio)
    , _sounds(sounds)
    , _game(game)
    , _registry(registry)
    , _channel(channel)
    , _canvas{settings.project.canvasWidth, settings.project.canvasHeight}
    , _frames(session, false)
    , _actions(std::move(actions))
    , _scene(
          _canvas, audio, sounds, [this](const std::string& text) { SendLog(text); }, font)
    , _profiler(game, _scene, channel, _canvas,
                [this] {
                    _actions.reset();
                    _actions.activate(_scene);
                })
    , _lastInput(std::chrono::steady_clock::now()) {
    _scene.SetApplicationCommand(_actions.application);
    _scene.SetSimulationHost(&_simulation);
}
WorkerSession::~WorkerSession() {
    try {
        _profiler.Cancel(false, false);
    } catch (...) {
    }
    _scene.ShutdownNoThrow();
}
float WorkerSession::Advance(float realSeconds) {
    if (_profiler.Busy()) return _profiler.Advance();
    ReplayDue();
    float seconds = _paused ? 0.0f : realSeconds * _speed;
    _sounds.SetHold(_paused); // A paused scene, or one seeking through steps, holds its live sounds in place
    const double fixedSeconds = _simulation.StepSeconds(StepSeconds);
    if (_seek) {
        const auto* runner = _simulation.Runner();
        const double remaining = _seek->target - _elapsed;
        const bool arrived =
            _seek->step ? (!runner || runner->StepIndex() >= *_seek->step) : remaining <= SeekTolerance;
        if (arrived || --_seek->stepsLeft < 0) {
            FinishSeek(); // This frame draws (and publishes) the arrived state
            seconds = 0.0f;
        } else {
            double step = _seek->step ? fixedSeconds : std::min(fixedSeconds, remaining);
            if (_applied < _timeline.size())
                step = std::min(step, _timeline[_applied].seconds - _elapsed); // Land on it
            seconds = static_cast<float>(step);
        }
    } else if (_step) {
        _step = false;
        if (auto* runner = _simulation.Runner()) runner->DropRemainder();
        seconds = static_cast<float>(fixedSeconds);
    }
    _elapsed += seconds;
    return seconds;
}
void WorkerSession::SendLog(const std::string& text) {
    if (ReportDroppedLogs() && _channel.TrySend({{}, _generation, 0, "log", {{"text", text}}})) return;
    ++_droppedLogs;
}
bool WorkerSession::ReportDroppedLogs() {
    if (!_droppedLogs) return true;
    const auto text = std::to_string(_droppedLogs) + " log line(s) dropped: the editor was not reading";
    if (!_channel.TrySend({{}, _generation, 0, "log", {{"text", text}}})) return false;
    _droppedLogs = 0;
    return true;
}
void WorkerSession::SendParameters() {
    _channel.SendLatest(
        {{}, _generation, 0, "parameters", {{"parameters", EncodeSceneParameters(_scene.Parameters())}}});
}
void WorkerSession::Reply(const WorkerMessage& message, const std::string& type, nlohmann::json data) {
    _channel.Send({{}, _generation, message.request, type, std::move(data)});
}
nlohmann::json WorkerSession::PlaybackState() const {
    nlohmann::json state{{"paused", _paused}, {"speed", _speed}, {"seconds", _elapsed}};
    if (auto simulation = _simulation.State(); !simulation.is_null()) state["simulation"] = std::move(simulation);
    return state;
}
void WorkerSession::Restart() {
    _profiler.Reset(_generation);
    _inputState = {};
    _statsStartedAt = 0.0;
    _publishedFrames = 0;
    _elapsed = 0.0;
    BeginTuningUse(); // The Inspector lists the tuning files this run reads
    _actions.reset();
    _actions.activate(_scene);
}
void WorkerSession::Rewind() { // Replays from the start: current parameters, recorded actions or the tape
    _simulation.PrepareRewind();
    Restart();
    _scene.Reset();
    _applied = 0;
}
void WorkerSession::StartSeek(const WorkerMessage& message) {
    if (const auto step = message.data.find("step"); step != message.data.end()) {
        auto* runner = _simulation.Runner();
        if (!runner) {
            Reply(message, "rejected", {{"text", "The open scene has no simulation"}});
            return;
        }
        const std::uint64_t amount = step->get<std::uint64_t>();
        const std::uint64_t target = message.data.value("relative", false) ? runner->StepIndex() + amount : amount;
        if (target > MaxSeekSteps) {
            Reply(message, "rejected", {{"text", "Seek step is out of range"}});
            return;
        }
        if (target < runner->StepIndex()) Rewind();
        runner = _simulation.Runner();
        if (!runner) { // The rebuilt scene hosts no simulation any more
            Reply(message, "rejected", {{"text", "The open scene has no simulation"}});
            return;
        }
        runner->DropRemainder(); // Whole steps from here, so the counter lands exactly
        BeginSeek(message, 0.0, target, static_cast<int>(target - runner->StepIndex()));
        return;
    }
    const double seconds = message.data.at("seconds").get<double>();
    const double target = message.data.value("relative", false) ? _elapsed + seconds : seconds;
    if (!std::isfinite(target) || target < 0.0 || target > MaxSeekSeconds) {
        Reply(message, "rejected", {{"text", "Seek time must be between 0 and 600 seconds"}});
        return;
    }
    if (target + SeekTolerance < _elapsed) Rewind();
    BeginSeek(message, target, std::nullopt,
              static_cast<int>(std::ceil((target - _elapsed) / _simulation.StepSeconds(StepSeconds))));
}
void WorkerSession::BeginSeek(const WorkerMessage& message, double target, std::optional<std::uint64_t> step,
                              int planned) {
    _paused = true;
    _step = false;
    _inputState = {};
    _inputState.cancelled = true;
    _seek = Seek{target, step, message.request, planned * SeekStepAllowance + SeekExtraSteps, _game.FrameExecution()};
    // Steps passed on the way are not played: their events stay silent until the seek arrives
    if (auto* runner = _simulation.Runner()) runner->SilenceEventsUntil(std::numeric_limits<std::uint64_t>::max());
    auto unthrottled = _seek->execution;
    unthrottled.fpsLimit = 0;
    unthrottled.vsync = false;
    _game.ConfigureFrameExecution(unthrottled);
}
void WorkerSession::Replay(const WorkerMessage& message) {
    const auto action = message.data.at("action").get<std::string>();
    const auto text = message.data.at("path").get<std::string>();
    const std::filesystem::path file(std::u8string(text.begin(), text.end()));
    if (action == "load") {
        if (_simulation.Load(file)) { // Paused at its first step, as a seek leaves a scene
            Restart();
            _scene.Reset();
            _timeline.clear();
            _applied = 0;
            _paused = true;
            _step = false;
            _inputState = {};
            _inputState.cancelled = true;
        }
    } else if (action == "save") {
        (void)_simulation.Save(file);
    } else {
        _simulation.SetLastError("Unknown replay action: " + action);
    }
    Reply(message, "state", PlaybackState());
}
void WorkerSession::ReplayDue() {
    bool replayed = false;
    while (_applied < _timeline.size() && _timeline[_applied].seconds <= _elapsed + SeekTolerance) {
        (void)_scene.Action(_timeline[_applied++].id); // Accepted when it was first run
        replayed = true;
    }
    if (replayed && _scene.CaptureParameters()) SendParameters();
}
void WorkerSession::FinishSeek() {
    if (auto* runner = _simulation.Runner()) runner->SilenceEventsUntil(runner->StepIndex());
    _game.ConfigureFrameExecution(_seek->execution);
    _channel.Send({{}, _generation, _seek->request, "state", PlaybackState()});
    _seek.reset();
}
void WorkerSession::Command(const WorkerMessage& message) {
    if (message.type == "shutdown") {
        _actions.quit();
        return;
    }
    if (message.type == "open") {
        if (!message.generation || message.generation <= _generation)
            throw std::runtime_error("Invalid open generation");
        const auto state = DecodeWorkerOpen(message.data);
        SetCrashContext(state.id);
        const auto* definition = _registry.Find(state.id);
        if (!definition) throw std::runtime_error("Selected scene is missing from the new catalog");
        _generation = message.generation;
        Restart();
        _simulation.Forget();
        _timeline.clear();
        _applied = 0;
        _scene.Open(*definition, state.parameters);
        _paused = state.paused;
        _speed = static_cast<float>(state.speed);
        _openRequest = message.request;
        _readyPending = true;
        return;
    }
    if (message.type == "reset") {
        if (_profiler.Busy()) {
            Reply(message, "rejected", {{"text", "Cancel the benchmark before resetting"}});
            return;
        }
        if (message.generation <= _generation || !_scene.HasContent()) return;
        _generation = message.generation;
        Restart();
        _simulation.Forget();
        _timeline.clear();
        _applied = 0;
        _scene.Reset();
        _openRequest = message.request;
        _readyPending = true;
        return;
    }
    if (message.generation != _generation || !_scene.HasContent()) return;
    if (message.type == "tuning-preview") { // A dragged value; the file write that ends the drag replaces it
        const auto preview = DecodeWorkerTuningPreview(message.data);
        (void)PreviewTuning(Utf8Path(preview.source), preview.at, preview.value);
        return;
    }
    if (message.type == "tuning-reload") { // The editor just wrote the file
        ReloadTuning(Utf8Path(message.data.at("source").get<std::string>()));
        return;
    }
    if (message.type == "profiler") {
        try {
            _profiler.Command(message);
        } catch (const std::exception& error) {
            Reply(message, "rejected", {{"text", error.what()}});
        }
        _inputState = {};
        _inputState.cancelled = true;
        return;
    }
    if (_profiler.Busy()) {
        if (message.type != "input") Reply(message, "rejected", {{"text", "Benchmark controls are locked"}});
        return;
    }
    if (message.type == "input") {
        AccumulateViewportInput(_inputState, DecodeWorkerInput(message.data));
        _lastInput = std::chrono::steady_clock::now();
    } else if (message.type == "pause") {
        _paused = message.data.at("paused").get<bool>();
        _inputState = {};
        _inputState.cancelled = true;
        Reply(message, "state", PlaybackState());
    } else if (message.type == "audio-capture") {
        try {
            const std::filesystem::path file = Utf8Path(message.data.at("file").get<std::string>());
            _audio.StartCapture(file, static_cast<float>(message.data.at("seconds").get<double>()));
            Reply(message, "state", PlaybackState());
        } catch (const std::exception& error) {
            Reply(message, "rejected", {{"text", error.what()}});
        }
    } else if (message.type == "playback") {
        const double speed = message.data.at("speed").get<double>();
        if (!ValidPlaybackSpeed(speed)) {
            Reply(message, "rejected", {{"text", "Invalid playback speed"}});
            return;
        }
        _speed = static_cast<float>(speed);
        _step = _paused && message.data.value("step", false);
        Reply(message, "state", PlaybackState());
    } else if (message.type == "seek") {
        StartSeek(message);
    } else if (message.type == "replay") {
        Replay(message);
    } else if (message.type == "sync") {
        _inspectionAt = -1.0; // The next frame reports inspection, so a reader after the sync sees current values
        Reply(message, "synced", PlaybackState());
    } else if (message.type == "parameter") {
        const auto id = message.data.at("id").get<std::string>();
        const auto value = DecodeSceneValue(message.data.at("value"));
        if (auto error = _scene.SetParameter(id, value); !error.empty())
            Reply(message, "rejected", {{"text", error}, {"id", id}});
        else {
            (void)_scene.CaptureParameters();
            Reply(message, "accepted", {{"id", id}, {"value", EncodeSceneValue(_scene.Parameters().at(id))}});
            SendParameters();
        }
    } else if (message.type == "action") {
        const auto id = message.data.at("id").get<std::string>();
        if (auto error = _scene.Action(id); !error.empty()) {
            Reply(message, "rejected", {{"text", error}});
            return;
        }
        if (!_simulation.Runner()) {    // With a simulation the action is a command on the tape, if it changes gameplay
            _timeline.resize(_applied); // Acting after a rewind replaces what followed
            _timeline.push_back({_elapsed, id});
            _applied = _timeline.size();
        }
        if (_scene.CaptureParameters()) SendParameters();
    } else {
        Reply(message, "rejected", {{"text", "Unsupported scene command: " + message.type}});
    }
}
void WorkerSession::PrepareFrame() {
    // Poll even during a seek: it also flushes this side's outgoing queue. Commands behind a seek, even those read
    // with it, wait in _queued and apply once it arrives.
    for (auto& message : _channel.Poll())
        _queued.push_back(std::move(message));
    (void)ReportDroppedLogs();
    while (!_seek && !_queued.empty()) {
        const auto message = std::move(_queued.front());
        _queued.pop_front();
        Command(message);
    }
    if (_channel.Closed()) _actions.quit();
    _profiler.Tick(_generation);
    if (const auto revision = TuningUseRevision(); _scene.HasContent() && revision != _tuningUse) {
        std::vector<std::string> sources;
        for (const auto& source : TuningUsed()) {
            const auto text = source.generic_u8string();
            sources.emplace_back(text.begin(), text.end());
        }
        if (_channel.TrySend({{}, _generation, 0, "tuning-used", EncodeWorkerTuningUsed(sources)}))
            _tuningUse = revision;
    }
}
DeviceInput WorkerSession::ReadInput() {
    if (_profiler.Busy()) {
        DeviceInput cancelled;
        cancelled.cancelled = true;
        return cancelled;
    }
    constexpr auto InputLease = std::chrono::seconds(1);
    if (std::chrono::steady_clock::now() - _lastInput > InputLease) {
        _inputState = {};
        _inputState.cancelled = true;
    }
    return ConsumeViewportInput(_inputState);
}
void WorkerSession::Capture() {
    if (_profiler.Busy()) return;
    if (_scene.CaptureParameters()) SendParameters();
}
void WorkerSession::CanvasReady(const Texture2D& texture) const {
    if (_profiler.Busy() || _seek) return;
    if (!_scene.HasContent()) return;
    if (auto simulation = _simulation.State(); simulation != _publishedSimulation) {
        _channel.SendLatest({{}, _generation, 0, "simulation", simulation});
        _publishedSimulation = std::move(simulation);
    }
    if (texture.width > WorkerMaxFrameWidth || texture.height > WorkerMaxFrameHeight)
        throw std::runtime_error("Scene canvas exceeds the bounded frame transport");
    const double startedAt = WorkerTimestamp();
    if (startedAt - _inspectionAt >= 0.1 || _readyPending) {
        const auto inspection = _scene.Inspect();
        if (inspection != _inspection || _readyPending)
            _channel.SendLatest({{}, _generation, 0, "inspection", EncodeSceneInspection(inspection)});
        _inspection = inspection;
        _inspectionAt = startedAt;
    }
    CapturedImage capture{LoadImageFromTexture(texture)};
    if (!capture.image.data) throw std::runtime_error("GPU frame readback failed");
    ImageFormat(&capture.image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ImageFlipVertical(&capture.image);
    const double capturedAt = WorkerTimestamp();
    const auto bytes = static_cast<std::size_t>(texture.width) * static_cast<std::size_t>(texture.height) * 4;
    const bool published = _frames.Publish(_generation, texture.width, texture.height,
                                           {static_cast<const std::byte*>(capture.image.data), bytes});
    const double publishedAt = WorkerTimestamp();
    if (!_statsStartedAt) _statsStartedAt = startedAt;
    if (published) ++_publishedFrames;
    if (publishedAt - _statsStartedAt >= 1.0) {
        _channel.SendLatest({{},
                             _generation,
                             0,
                             "metrics",
                             {{"captureMs", (capturedAt - startedAt) * 1000.0},
                              {"publishMs", (publishedAt - capturedAt) * 1000.0},
                              {"fps", static_cast<double>(_publishedFrames) / (publishedAt - _statsStartedAt)}}});
        _publishedFrames = 0;
        _statsStartedAt = publishedAt;
    }
    if (published && _readyPending) {
        _channel.Send({{},
                       _generation,
                       _openRequest,
                       "ready",
                       {{"parameters", EncodeSceneParameters(_scene.Parameters())},
                        {"paused", _paused},
                        {"speed", _speed},
                        {"canvas", nlohmann::json::array({_canvas.x, _canvas.y})},
                        {"simulation", _simulation.State()}}});
        _readyPending = false;
    }
}
void WorkerSession::Stop() {
    _profiler.Cancel(false, false);
    _scene.Shutdown();
    if (!_channel.Closed()) {
        _channel.Send({{}, _generation, 0, "stopped", nlohmann::json::object()});
        (void)_channel.Poll();
    }
}
