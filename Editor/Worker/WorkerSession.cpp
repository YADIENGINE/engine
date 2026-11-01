#include "Editor/Worker/WorkerSession.h"
#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <span>
#include <utility>
#include <stdexcept>
#include "Editor/Internals/Viewport/ViewportInputRouter.h"
#include "Runtime/Game.h"

namespace {
constexpr double StepSeconds = 1.0 / 60.0; // One Step, and one seek step
constexpr double MaxSeekSeconds = 600.0;
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
} // namespace

WorkerSession::WorkerSession(Game& game, const GameSettings& settings, const SceneRegistry& registry,
                             ControlChannel& channel, const std::string& session, const FontFace* font,
                             AudioSystem& audio, WorkerSessionActions actions)
    : _game(game)
    , _registry(registry)
    , _channel(channel)
    , _canvas{settings.canvasWidth, settings.canvasHeight}
    , _frames(session, false)
    , _actions(std::move(actions))
    , _scene(
          _canvas, audio,
          [this](const std::string& text) { _channel.Send({{}, _generation, 0, "log", {{"text", text}}}); }, font)
    , _profiler(game, _scene, channel, _canvas,
                [this] {
                    _actions.reset();
                    _actions.activate(_scene);
                })
    , _lastInput(std::chrono::steady_clock::now()) {
    _scene.SetApplicationCommand(_actions.application);
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
    if (_seek) {
        const double remaining = _seek->target - _elapsed;
        if (remaining <= SeekTolerance || --_seek->stepsLeft < 0) {
            FinishSeek(); // This frame draws (and publishes) the arrived state
            seconds = 0.0f;
        } else {
            double step = std::min(StepSeconds, remaining);
            if (_applied < _timeline.size())
                step = std::min(step, _timeline[_applied].seconds - _elapsed); // Land on it
            seconds = static_cast<float>(step);
        }
    } else if (_step) {
        _step = false;
        seconds = static_cast<float>(StepSeconds);
    }
    _elapsed += seconds;
    return seconds;
}
void WorkerSession::Reply(const WorkerMessage& message, const std::string& type, nlohmann::json data) {
    _channel.Send({{}, _generation, message.request, type, std::move(data)});
}
nlohmann::json WorkerSession::PlaybackState() const {
    return {{"paused", _paused}, {"speed", _speed}, {"seconds", _elapsed}};
}
void WorkerSession::Restart() {
    _profiler.Reset(_generation);
    _inputState = {};
    _statsStartedAt = 0.0;
    _publishedFrames = 0;
    _elapsed = 0.0;
    _actions.reset();
    _actions.activate(_scene);
}
void WorkerSession::StartSeek(const WorkerMessage& message) {
    const double seconds = message.data.at("seconds").get<double>();
    const double target = message.data.value("relative", false) ? _elapsed + seconds : seconds;
    if (!std::isfinite(target) || target < 0.0 || target > MaxSeekSeconds) {
        Reply(message, "rejected", {{"text", "Seek time must be between 0 and 600 seconds"}});
        return;
    }
    if (target + SeekTolerance < _elapsed) { // Rewinding replays from the start: current parameters, recorded actions
        Restart();
        _scene.Reset();
        _applied = 0;
    }
    _paused = true;
    _step = false;
    _inputState = {};
    _inputState.cancelled = true;
    const int planned = static_cast<int>(std::ceil((target - _elapsed) / StepSeconds));
    _seek = Seek{target, message.request, planned * SeekStepAllowance + SeekExtraSteps, _game.FrameExecution()};
    auto unthrottled = _seek->execution;
    unthrottled.fpsLimit = 0;
    unthrottled.vsync = false;
    _game.ConfigureFrameExecution(unthrottled);
}
void WorkerSession::ReplayDue() {
    bool replayed = false;
    while (_applied < _timeline.size() && _timeline[_applied].seconds <= _elapsed + SeekTolerance) {
        (void)_scene.Action(_timeline[_applied++].id); // Accepted when it was first run
        replayed = true;
    }
    if (replayed && _scene.CaptureParameters())
        _channel.Send({{}, _generation, 0, "parameters", {{"parameters", EncodeSceneParameters(_scene.Parameters())}}});
}
void WorkerSession::FinishSeek() {
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
        const auto* definition = _registry.Find(state.id);
        if (!definition) throw std::runtime_error("Selected scene is missing from the new catalog");
        _generation = message.generation;
        Restart();
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
        _timeline.clear();
        _applied = 0;
        _scene.Reset();
        _openRequest = message.request;
        _readyPending = true;
        return;
    }
    if (message.generation != _generation || !_scene.HasContent()) return;
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
            _channel.Send(
                {{}, _generation, 0, "parameters", {{"parameters", EncodeSceneParameters(_scene.Parameters())}}});
        }
    } else if (message.type == "action") {
        const auto id = message.data.at("id").get<std::string>();
        if (auto error = _scene.Action(id); !error.empty()) {
            Reply(message, "rejected", {{"text", error}});
            return;
        }
        _timeline.resize(_applied); // Acting after a rewind replaces what followed
        _timeline.push_back({_elapsed, id});
        _applied = _timeline.size();
        if (_scene.CaptureParameters())
            _channel.Send(
                {{}, _generation, 0, "parameters", {{"parameters", EncodeSceneParameters(_scene.Parameters())}}});
    } else {
        Reply(message, "rejected", {{"text", "Unsupported scene command: " + message.type}});
    }
}
void WorkerSession::PrepareFrame() {
    // Commands behind a seek, even those read with it, apply once it arrives
    if (!_seek)
        for (auto& message : _channel.Poll())
            _queued.push_back(std::move(message));
    while (!_seek && !_queued.empty()) {
        const auto message = std::move(_queued.front());
        _queued.pop_front();
        Command(message);
    }
    if (_channel.Closed()) _actions.quit();
    _profiler.Tick(_generation);
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
    if (_scene.CaptureParameters())
        _channel.Send({{}, _generation, 0, "parameters", {{"parameters", EncodeSceneParameters(_scene.Parameters())}}});
}
void WorkerSession::CanvasReady(const Texture2D& texture) const {
    if (_profiler.Busy() || _seek) return;
    if (!_scene.HasContent()) return;
    if (texture.width > WorkerMaxFrameWidth || texture.height > WorkerMaxFrameHeight)
        throw std::runtime_error("Scene canvas exceeds the bounded frame transport");
    const double startedAt = WorkerTimestamp();
    if (startedAt - _inspectionAt >= 0.1 || _readyPending) {
        const auto inspection = _scene.Inspect();
        if (inspection != _inspection || _readyPending)
            _channel.Send({{}, _generation, 0, "inspection", EncodeSceneInspection(inspection)});
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
        _channel.Send({{},
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
                        {"canvas", nlohmann::json::array({_canvas.x, _canvas.y})}}});
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
