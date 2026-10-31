#pragma once
#include <chrono>
#include <functional>
#include "Modules/Profiler/Profiler.h"
#include "Runtime/FrameExecutionSettings.h"
#include "Editor/Api/PreviewRegistry.h"
#include "Editor/Internals/Protocol/PreviewProtocol.h"
#include "Editor/Internals/Session/BenchmarkState.h"

class ControlChannel;
class PreviewScene;

class PreviewProfiler {
public:
    PreviewProfiler(Game& game, PreviewScene& scene, ControlChannel& channel, Vec2 canvas,
                    std::function<void()> activate);
    ~PreviewProfiler();
    void Command(const PreviewMessage& message);
    void Tick(std::uint64_t generation);
    void Cancel(bool notify = true, bool restorePreview = true);
    void Reset(std::uint64_t generation);
    [[nodiscard]] bool Busy() const;
    [[nodiscard]] float Advance() const;

private:
    using Clock = std::chrono::steady_clock;
    void Start(const PreviewMessage& message);
    void StartRun();
    void Restore(bool preview = true);
    void State(const std::string& error = {});
    void PublishCapture(const Profiler::Capture& capture, const std::string& kind);
    void PublishNext();
    Game& _game;
    PreviewScene& _scene;
    ControlChannel& _channel;
    Vec2 _canvas;
    std::function<void()> _activate;
    Profiler _profiler{10000};
    Profiler::Metadata _runtimeMetadata;
    Profiler::Capture _capture;
    BenchmarkState _schedule;
    const PreviewDefinition* _savedDefinition = nullptr;
    PreviewParameters _savedParameters;
    std::vector<int> _order;
    std::string _phase = "Idle";
    std::uint64_t _generation = 0, _captureId = 0;
    std::size_t _run = 0, _warmup = 0;
    bool _live = false;
    Clock::time_point _publishAt{}, _collectUntil{};
    std::optional<Profiler::Capture> _publication;
    std::string _publicationKind;
    std::size_t _publicationRun = 0, _publicationOffset = 0;
    bool _publicationHeader = false;
};
