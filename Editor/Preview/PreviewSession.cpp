#include "Editor/Preview/PreviewSession.h"
#include <raylib.h>
#include <span>
#include <utility>
#include <stdexcept>
#include "Editor/Internals/Viewport/PreviewInputRouter.h"

namespace {
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

PreviewSession::PreviewSession(Game& game, const GameSettings& settings, const PreviewRegistry& registry,
                               ControlChannel& channel, const std::string& session, const TextRenderer& text,
                               AudioSystem& audio, PreviewSessionActions actions)
    : _registry(registry)
    , _channel(channel)
    , _canvas{settings.canvasWidth, settings.canvasHeight}
    , _frames(session, false)
    , _actions(std::move(actions))
    , _scene(_canvas, text, audio,
             [this](const std::string& text) { _channel.Send({{}, _generation, 0, "log", {{"text", text}}}); })
    , _profiler(game, _scene, channel, _canvas,
                [this] {
                    _actions.reset();
                    _actions.activate(_scene);
                })
    , _lastInput(std::chrono::steady_clock::now()) {
    _scene.SetApplicationCommand(_actions.application);
}
PreviewSession::~PreviewSession() {
    try {
        _profiler.Cancel(false, false);
    } catch (...) {
    }
    _scene.ShutdownNoThrow();
}
float PreviewSession::Advance(float realSeconds) {
    if (_profiler.Busy()) return _profiler.Advance();
    if (_step) {
        _step = false;
        return 1.0f / 60.0f;
    }
    return _paused ? 0.0f : realSeconds * _speed;
}
void PreviewSession::Reply(const PreviewMessage& message, const std::string& type, nlohmann::json data) {
    _channel.Send({{}, _generation, message.request, type, std::move(data)});
}
void PreviewSession::Command(const PreviewMessage& message) {
    if (message.type == "shutdown") {
        _actions.quit();
        return;
    }
    if (message.type == "open") {
        if (!message.generation || message.generation <= _generation)
            throw std::runtime_error("Invalid open generation");
        const auto state = DecodePreviewOpen(message.data);
        const auto* definition = _registry.Find(state.id);
        if (!definition) throw std::runtime_error("Selected preview is missing from the new catalog");
        _generation = message.generation;
        _profiler.Reset(_generation);
        _inputState = {};
        _statsStartedAt = 0.0;
        _publishedFrames = 0;
        _actions.reset();
        _actions.activate(_scene);
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
        _profiler.Reset(_generation);
        _inputState = {};
        _statsStartedAt = 0.0;
        _publishedFrames = 0;
        _actions.reset();
        _actions.activate(_scene);
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
        AccumulatePreviewInput(_inputState, DecodePreviewInput(message.data));
        _lastInput = std::chrono::steady_clock::now();
    } else if (message.type == "pause") {
        _paused = message.data.at("paused").get<bool>();
        _inputState = {};
        _inputState.cancelled = true;
        Reply(message, "state", {{"paused", _paused}});
    } else if (message.type == "playback") {
        const double speed = message.data.at("speed").get<double>();
        if (!ValidPlaybackSpeed(speed)) {
            Reply(message, "rejected", {{"text", "Invalid playback speed"}});
            return;
        }
        _speed = static_cast<float>(speed);
        _step = _paused && message.data.value("step", false);
        Reply(message, "state", {{"paused", _paused}, {"speed", _speed}});
    } else if (message.type == "parameter") {
        const auto id = message.data.at("id").get<std::string>();
        const auto value = DecodePreviewValue(message.data.at("value"));
        if (auto error = _scene.SetParameter(id, value); !error.empty())
            Reply(message, "rejected", {{"text", error}, {"id", id}});
        else {
            (void)_scene.CaptureParameters();
            Reply(message, "accepted", {{"id", id}, {"value", EncodePreviewValue(_scene.Parameters().at(id))}});
            _channel.Send(
                {{}, _generation, 0, "parameters", {{"parameters", EncodePreviewParameters(_scene.Parameters())}}});
        }
    } else if (message.type == "action") {
        if (auto error = _scene.Action(message.data.at("id").get<std::string>()); !error.empty())
            Reply(message, "rejected", {{"text", error}});
        else if (_scene.CaptureParameters())
            _channel.Send(
                {{}, _generation, 0, "parameters", {{"parameters", EncodePreviewParameters(_scene.Parameters())}}});
    } else {
        Reply(message, "rejected", {{"text", "Unsupported preview command: " + message.type}});
    }
}
void PreviewSession::PrepareFrame() {
    for (const auto& message : _channel.Poll())
        Command(message);
    if (_channel.Closed()) _actions.quit();
    _profiler.Tick(_generation);
}
DeviceInput PreviewSession::ReadInput() {
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
    return ConsumePreviewInput(_inputState);
}
void PreviewSession::Capture() {
    if (_profiler.Busy()) return;
    if (_scene.CaptureParameters())
        _channel.Send(
            {{}, _generation, 0, "parameters", {{"parameters", EncodePreviewParameters(_scene.Parameters())}}});
}
void PreviewSession::CanvasReady(const Texture2D& texture) const {
    if (_profiler.Busy()) return;
    if (!_scene.HasContent()) return;
    if (texture.width > PreviewMaxFrameWidth || texture.height > PreviewMaxFrameHeight)
        throw std::runtime_error("Preview canvas exceeds the bounded frame transport");
    const double startedAt = PreviewTimestamp();
    if (startedAt - _inspectionAt >= 0.1 || _readyPending) {
        const auto inspection = _scene.Inspect();
        if (inspection != _inspection || _readyPending)
            _channel.Send({{}, _generation, 0, "inspection", EncodePreviewInspection(inspection)});
        _inspection = inspection;
        _inspectionAt = startedAt;
    }
    CapturedImage capture{LoadImageFromTexture(texture)};
    if (!capture.image.data) throw std::runtime_error("GPU frame readback failed");
    ImageFormat(&capture.image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ImageFlipVertical(&capture.image);
    const double capturedAt = PreviewTimestamp();
    const auto bytes = static_cast<std::size_t>(texture.width) * static_cast<std::size_t>(texture.height) * 4;
    const bool published = _frames.Publish(_generation, texture.width, texture.height,
                                           {static_cast<const std::byte*>(capture.image.data), bytes});
    const double publishedAt = PreviewTimestamp();
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
                       {{"parameters", EncodePreviewParameters(_scene.Parameters())},
                        {"paused", _paused},
                        {"speed", _speed},
                        {"canvas", nlohmann::json::array({_canvas.x, _canvas.y})}}});
        _readyPending = false;
    }
}
void PreviewSession::Stop() {
    _profiler.Cancel(false, false);
    _scene.Shutdown();
    if (!_channel.Closed()) {
        _channel.Send({{}, _generation, 0, "stopped", nlohmann::json::object()});
        (void)_channel.Poll();
    }
}
