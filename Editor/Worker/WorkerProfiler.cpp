#include "Editor/Worker/WorkerProfiler.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include "Editor/Worker/WorkerScene.h"
#include "Editor/Internals/Platform/Windows/ControlChannel.h"
#include "Runtime/Game.h"

WorkerProfiler::WorkerProfiler(Game& game, WorkerScene& scene, ControlChannel& channel, Vec2 canvas,
                               std::function<void()> activate)
    : _game(game)
    , _scene(scene)
    , _channel(channel)
    , _canvas(canvas)
    , _activate(std::move(activate)) {
    _profiler.Attach(game);
    _runtimeMetadata = _profiler.RuntimeMetadata();
    _scene.SetProfiler(&_profiler);
}
WorkerProfiler::~WorkerProfiler() {
    try {
        Cancel(false, false);
    } catch (...) {
    }
    _scene.SetProfiler(nullptr);
    _profiler.Detach();
}
bool WorkerProfiler::Busy() const {
    return (_publication && _publicationKind == "benchmark") || _phase == "Preparing" || _phase == "Warmup" ||
           _phase == "Measuring" || _phase == "Collecting GPU Results";
}
float WorkerProfiler::Advance() const {
    return _phase == "Warmup" || _phase == "Measuring" ? static_cast<float>(_capture.metadata.recipe.stepSeconds)
                                                       : 0.0f;
}
void WorkerProfiler::State(const std::string& error) {
    const auto frame = _phase == "Warmup" ? _warmup : _profiler.BufferedFrames();
    _channel.Send({{},
                   _generation,
                   0,
                   "profiler-state",
                   {{"captureId", _captureId},
                    {"phase", _phase},
                    {"live", _live},
                    {"busy", Busy()},
                    {"run", _run},
                    {"runs", _order.size()},
                    {"frame", frame},
                    {"target", _phase == "Warmup" ? _capture.metadata.recipe.warmupFrames
                                                  : _capture.metadata.recipe.measureFrames},
                    {"gpuSupported", _profiler.GpuSupported()},
                    {"error", error}}});
}
void WorkerProfiler::Command(const WorkerMessage& message) {
    _generation = message.generation;
    const auto operation = message.data.at("operation").get<std::string>();
    if (operation == "start") {
        if (Busy()) throw std::runtime_error("A benchmark is already running");
        try {
            Start(message);
        } catch (const std::exception& error) {
            _captureId = message.data.value("captureId", std::uint64_t{});
            try {
                if (_savedDefinition) Restore();
            } catch (...) {
            }
            _phase = "Failed";
            _profiler.SetEnabled(_live);
            State(error.what());
        }
        return;
    }
    if (operation == "cancel") {
        if (message.data.value("captureId", std::uint64_t{}) == _captureId) Cancel();
        return;
    }
    if (operation == "enable" && !Busy()) {
        _publication.reset();
        _live = message.data.at("enabled").get<bool>();
        _profiler.ClearFrames();
        _profiler.SetEnabled(_live);
        _phase = "Idle";
        State();
    }
    if (operation == "glow" && !Busy()) {
        const int level = message.data.at("level").get<int>();
        if (level < 0 || level > 2) throw std::invalid_argument("Invalid glow quality");
        RenderQuality quality = _game.AppliedGraphics().render;
        quality.glow = static_cast<GlowQuality>(level);
        _game.OverrideRenderQuality(quality); // The player's options stay untouched
        _profiler.ClearFrames();
        State();
    }
}
void WorkerProfiler::Start(const WorkerMessage& message) {
    if (Busy()) throw std::runtime_error("A benchmark is already running");
    const auto* definition = _scene.Definition();
    if (!definition || !definition->benchmark) throw std::runtime_error("This scene has no benchmark contract");
    Profiler::Recipe recipe{message.data.at("warmup").get<int>(), message.data.at("frames").get<int>(),
                            message.data.at("pairs").get<int>(), 1.0 / 60.0};
    Profiler::ValidateRecipe(recipe);
    _captureId = message.data.at("captureId").get<std::uint64_t>();
    if (!_captureId) throw std::runtime_error("Invalid capture ID");
    _schedule.Start(recipe, _game.FrameExecution());
    _savedDefinition = definition;
    _savedParameters = _scene.Parameters();
    _publication.reset();
    _capture = {};
    _capture.metadata = _runtimeMetadata;
    auto& metadata = _capture.metadata;
    metadata.scenario = definition->benchmark->id;
    metadata.scenarioVersion = definition->benchmark->version;
    metadata.parameters = EncodeSceneParameters(_savedParameters).dump();
    metadata.width = _canvas.x;
    metadata.height = _canvas.y;
    metadata.recipe = recipe;
    if (const auto seed = _savedParameters.find("seed"); seed != _savedParameters.end())
        if (const auto* value = std::get_if<std::int64_t>(&seed->second))
            metadata.seed = static_cast<std::uint32_t>(*value);
    const GraphicsSettings graphics = _game.AppliedGraphics();
    metadata.graphics = nlohmann::json{{"glow", graphics.render.glow},
                                       {"bloom", graphics.render.bloom},
                                       {"background", graphics.render.backgroundEffects},
                                       {"particles", graphics.fx.particlePercent},
                                       {"shake", graphics.fx.shake},
                                       {"motion", graphics.fx.motion},
                                       {"flashes", graphics.fx.flashes}}
                            .dump();
    auto variants = nlohmann::json::array();
    for (const auto& variant : definition->benchmark->variants)
        variants.push_back({{"name", variant.name}, {"parameters", EncodeSceneParameters(variant.parameters)}});
    metadata.execution = nlohmann::json{{"fpsLimit", 0}, {"vsync", false}, {"variants", variants}}.dump();
    _capture.scopes = _profiler.ScopeNames();
    _capture.counters = _profiler.CounterNames();
    _capture.variants = {definition->benchmark->variants[0].name, definition->benchmark->variants[1].name};
    _order = Profiler::RunOrder(recipe);
    _run = 0;
    _phase = "Preparing";
    State();
    try {
        StartRun();
    } catch (...) {
        Restore();
        _phase = "Failed";
        State("Could not prepare benchmark");
        throw;
    }
}
void WorkerProfiler::StartRun() {
    _schedule.BeginRun();
    const auto& variant = _savedDefinition->benchmark->variants[static_cast<std::size_t>(_order.at(_run))];
    SceneParameters parameters = _savedParameters;
    for (const auto& [key, value] : variant.parameters)
        parameters[key] = value;
    _profiler.SetEnabled(false);
    _activate();
    _scene.Open(*_savedDefinition, parameters);
    _game.ConfigureFrameExecution({0, false});
    _profiler.ClearFrames();
    _warmup = 0;
    _profiler.SetEnabled(true);
    _phase = _schedule.Phase() == BenchmarkPhase::Warmup ? "Warmup" : "Measuring";
    _publishAt = Clock::now();
    State();
}
void WorkerProfiler::Restore(bool scene) {
    _profiler.SetEnabled(false);
    const auto* definition = std::exchange(_savedDefinition, nullptr);
    if (const auto execution = _schedule.TakeSavedExecution()) _game.ConfigureFrameExecution(*execution);
    if (scene && definition) {
        _activate();
        _scene.Open(*definition, _savedParameters);
    }
    _profiler.ClearFrames();
    _profiler.SetEnabled(_live);
}
void WorkerProfiler::Cancel(bool notify, bool restoreScene) {
    if (!Busy()) return;
    _capture.completed = false;
    _schedule.Cancel();
    _publication.reset();
    try {
        if (_savedDefinition) Restore(restoreScene);
    } catch (...) {
        _phase = "Failed";
        _profiler.SetEnabled(_live);
        if (notify) State("Could not restore scene");
        return;
    }
    _profiler.SetEnabled(_live);
    _phase = "Cancelled";
    if (notify) State();
}
void WorkerProfiler::Reset(std::uint64_t generation) {
    Cancel(false, false);
    _publication.reset();
    _generation = generation;
    _captureId = 0;
    _phase = "Idle";
    _order.clear();
    _profiler.ClearFrames();
    _profiler.SetEnabled(_live);
}
void WorkerProfiler::PublishCapture(const Profiler::Capture& capture, const std::string& kind) {
    _publication = capture;
    _publicationKind = kind;
    _publicationRun = _publicationOffset = 0;
    _publicationHeader = false;
}
void WorkerProfiler::PublishNext() {
    if (!_publication) return;
    Profiler::Capture part;
    part.metadata = _publication->metadata;
    part.scopes = _publication->scopes;
    part.counters = _publication->counters;
    part.variants = _publication->variants;
    part.completed = _publication->completed;
    const auto encode = [&] {
        return nlohmann::json::parse(
            JsonIO::Serialize(Profiler::Encode(part), {.limits = {.maxBytes = 128 * 1024 * 1024}}));
    };
    if (!_publicationHeader) {
        _channel.Send({{},
                       _generation,
                       0,
                       "profiler-result",
                       {{"captureId", _captureId},
                        {"kind", _publicationKind},
                        {"runCount", _publication->runs.size()},
                        {"header", encode()}}});
        _publicationHeader = true;
    }
    for (int budget = 0; budget < 4 && _channel.QueuedBytes() < 32768; ++budget) {
        if (_publicationRun >= _publication->runs.size()) {
            _channel.Send(
                {{}, _generation, 0, "profiler-complete", {{"captureId", _captureId}, {"kind", _publicationKind}}});
            const bool benchmark = _publicationKind == "benchmark";
            _publication.reset();
            _profiler.SetEnabled(_live);
            if (benchmark) State();
            return;
        }
        const auto& source = _publication->runs[_publicationRun];
        part.runs = {{source.variant, source.repeat, {}}};
        const auto end = std::min(_publicationOffset + 16, source.frames.size());
        part.runs.front().frames.assign(source.frames.begin() + static_cast<std::ptrdiff_t>(_publicationOffset),
                                        source.frames.begin() + static_cast<std::ptrdiff_t>(end));
        _channel.Send({{},
                       _generation,
                       0,
                       "profiler-chunk",
                       {{"captureId", _captureId},
                        {"kind", _publicationKind},
                        {"run", _publicationRun},
                        {"offset", _publicationOffset},
                        {"capture", encode()}}});
        _publicationOffset = end;
        if (end == source.frames.size()) {
            ++_publicationRun;
            _publicationOffset = 0;
        }
    }
}
void WorkerProfiler::Tick(std::uint64_t generation) {
    _generation = generation;
    if (!_generation || !_scene.HasContent()) return;
    try {
        if (_publication) {
            PublishNext();
            return;
        }
        if (Busy()) {
            if (_phase == "Warmup") {
                const bool measured = _schedule.AddWarmup(_profiler.TakeFrames().size());
                _warmup = _schedule.WarmupCount();
                if (measured) {
                    _profiler.ClearFrames();
                    _phase = "Measuring";
                    State();
                }
            } else if (_phase == "Measuring") {
                if (_schedule.Measured(_profiler.BufferedFrames())) {
                    _profiler.SetEnabled(false);
                    _phase = "Collecting GPU Results";
                    _collectUntil = Clock::now() + std::chrono::seconds(2);
                    State();
                }
            } else if (_phase == "Collecting GPU Results") {
                _profiler.PollGpu();
                if (!_profiler.PendingGpu() || Clock::now() >= _collectUntil) {
                    auto frames = _profiler.TakeFrames();
                    if (frames.size() > static_cast<std::size_t>(_capture.metadata.recipe.measureFrames))
                        frames.resize(static_cast<std::size_t>(_capture.metadata.recipe.measureFrames));
                    _capture.runs.push_back({_order[_run], static_cast<int>(_run / 2), std::move(frames)});
                    const bool next = _schedule.FinishRun();
                    _run = _schedule.RunIndex();
                    if (next)
                        StartRun();
                    else {
                        _capture.completed = true;
                        _capture.scopes = _profiler.ScopeNames();
                        _capture.counters = _profiler.CounterNames();
                        Restore();
                        _phase = "Completed";
                        _profiler.SetEnabled(false);
                        PublishCapture(_capture, "benchmark");
                        State();
                    }
                }
            }
            if (Clock::now() - _publishAt >= std::chrono::milliseconds(200)) {
                _publishAt = Clock::now();
                State();
            }
        } else if (_live && Clock::now() - _publishAt >= std::chrono::milliseconds(100)) {
            _publishAt = Clock::now();
            _profiler.PollGpu();
            auto frames = _profiler.TakeFrames();
            if (frames.size() > 120) frames.erase(frames.begin(), frames.end() - 120);
            if (!frames.empty()) {
                Profiler::Capture live;
                live.metadata = _runtimeMetadata;
                live.metadata.scenario = _scene.Definition()->id;
                live.metadata.width = _canvas.x;
                live.metadata.height = _canvas.y;
                live.scopes = _profiler.ScopeNames();
                live.counters = _profiler.CounterNames();
                live.runs = {{1, 0, std::move(frames)}};
                PublishCapture(live, "live");
            }
        }
    } catch (const std::exception& error) {
        if (Busy()) {
            try {
                if (_savedDefinition) Restore();
            } catch (...) {
            }
            _publication.reset();
            _profiler.SetEnabled(_live);
            _phase = "Failed";
            State(error.what());
        } else {
            _live = false;
            _profiler.SetEnabled(false);
            State(error.what());
        }
    }
}
