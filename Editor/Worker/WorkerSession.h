#pragma once
#include <raylib.h>
#include <chrono>
#include <deque>
#include <optional>
#include "Editor/Internals/Platform/Windows/ControlChannel.h"
#include "Editor/Internals/Platform/Windows/SharedFrames.h"
#include "Editor/Worker/WorkerScene.h"
#include "Runtime/FrameExecutionSettings.h"
#include "Runtime/GameSettings.h"
#include "Editor/Worker/WorkerProfiler.h"

struct WorkerSessionActions {
    std::function<void()> quit;
    std::function<void()> reset;
    std::function<void(WorkerScene&)> activate;
    std::function<std::string(const std::string&, const SceneParameters&)> application;
};

// Transport and scene lifecycle, reusable by a project's actual Game without another window or loop.
class WorkerSession {
public:
    WorkerSession(Game& game, const GameSettings& settings, const SceneRegistry& registry, ControlChannel& channel,
                  const std::string& session, const FontFace* font, AudioSystem& audio, WorkerSessionActions actions);
    ~WorkerSession();
    [[nodiscard]] WorkerScene& Scene() { return _scene; }
    [[nodiscard]] const WorkerScene& Scene() const { return _scene; }
    [[nodiscard]] DeviceInput ReadInput();
    void PrepareFrame();
    void Capture();
    void CanvasReady(const Texture2D& texture) const;
    void Stop();
    [[nodiscard]] float Advance(float realSeconds);

private:
    // A seek in progress: playback advances in fixed steps, unthrottled and without frame readback, until the clock
    // reaches the target; commands after it wait (their replies then describe the arrived state).
    struct Seek {
        double target = 0.0;
        std::uint64_t request = 0;
        int stepsLeft = 0;
        FrameExecutionSettings execution;
    };
    void Command(const WorkerMessage& message);
    void Reply(const WorkerMessage& message, const std::string& type, nlohmann::json data);
    void Restart();
    void StartSeek(const WorkerMessage& message);
    void FinishSeek();
    // Re-runs recorded actions whose time has come again after a rewind.
    void ReplayDue();
    [[nodiscard]] nlohmann::json PlaybackState() const;
    Game& _game;
    const SceneRegistry& _registry;
    ControlChannel& _channel;
    Vec2 _canvas;
    mutable SharedFrames _frames;
    WorkerSessionActions _actions;
    WorkerScene _scene;
    mutable WorkerProfiler _profiler;
    DeviceInput _inputState;
    std::chrono::steady_clock::time_point _lastInput;
    std::uint64_t _generation = 0, _openRequest = 0;
    bool _paused = false, _step = false;
    float _speed = 1.0f;
    double _elapsed = 0.0; // Playback seconds since open / reset (the seek and sync clock)
    std::optional<Seek> _seek;
    std::deque<WorkerMessage> _queued; // Read but not yet applied (waiting behind a seek)
    // Actions since open / reset at their playback time; a rewind replays them as the clock passes them again.
    struct TimedAction {
        double seconds = 0.0;
        std::string id;
    };
    std::vector<TimedAction> _timeline;
    std::size_t _applied = 0; // Entries of _timeline already in effect
    mutable bool _readyPending = false;
    mutable double _statsStartedAt = 0, _inspectionAt = -1;
    mutable std::uint64_t _publishedFrames = 0;
    mutable SceneInspection _inspection;
};
