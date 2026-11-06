#pragma once
#include <raylib.h>
#include <chrono>
#include <deque>
#include <optional>
#include "Editor/Internals/Platform/Windows/ControlChannel.h"
#include "Editor/Internals/Platform/Windows/SharedFrames.h"
#include "Editor/Internals/Session/WorkerSimulation.h"
#include "Editor/Worker/WorkerScene.h"
#include "Runtime/FrameExecutionSettings.h"
#include "Runtime/GameSettings.h"
#include "Editor/Worker/WorkerProfiler.h"

struct WorkerSessionActions {
    std::function<void()> quit;
    std::function<void()> reset;
    std::function<void(WorkerScene&)> activate;
};

// Transport and scene lifecycle, reusable by a project's actual Game without another window or loop.
class WorkerSession {
public:
    // Scenes get the game's services (Game::Services()) with the editor's log and simulation host.
    WorkerSession(Game& game, const GameSettings& settings, const SceneRegistry& registry, ControlChannel& channel,
                  const std::string& session, WorkerSessionActions actions);
    ~WorkerSession();
    [[nodiscard]] WorkerScene& Scene() { return _scene; }
    [[nodiscard]] const WorkerScene& Scene() const { return _scene; }
    // What a custom Game adapter hands its own scenes, so their HostedSimulation reaches the editor's tape.
    [[nodiscard]] const SceneServices& Services() const { return _scene.Services(); }
    [[nodiscard]] DeviceInput ReadInput();
    void PrepareFrame();
    void Capture();
    void CanvasReady(const Texture2D& texture) const;
    void Stop();
    [[nodiscard]] float Advance(float realSeconds);

private:
    AudioSystem& _audio;
    SoundPlayer& _sounds;
    // A seek in progress: playback advances in fixed steps, unthrottled and without frame readback, until the clock
    // or the simulation's step counter reaches the target; commands after it wait (their replies then describe the
    // arrived state).
    struct Seek {
        double target = 0.0;
        std::optional<std::uint64_t> step; // Set: the target is this simulation step, not a playback time
        std::uint64_t request = 0;
        int stepsLeft = 0;
        FrameExecutionSettings execution;
    };
    void Command(const WorkerMessage& message);
    void Reply(const WorkerMessage& message, const std::string& type, nlohmann::json data);
    // Scene logs are best effort: a busy channel drops them and later reports how many.
    void SendLog(const std::string& text);
    [[nodiscard]] bool ReportDroppedLogs();
    void SendParameters();
    void Restart();
    void StartSeek(const WorkerMessage& message);
    void BeginSeek(const WorkerMessage& message, double target, std::optional<std::uint64_t> step, int planned);
    void Rewind();
    void Replay(const WorkerMessage& message);
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
    WorkerSimulation _simulation;
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
    std::size_t _droppedLogs = 0;
    std::uint64_t _tuningUse = 0; // TuningUseRevision last sent as "tuning-used"
    // Actions since open / reset at their playback time; a rewind replays them as the clock passes them again. A scene
    // with a simulation keeps none here: its actions are commands on the tape (D26).
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
    mutable nlohmann::json _publishedSimulation;
};
