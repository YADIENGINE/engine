#pragma once
#include <string>
#include "Scene/SceneInspection.h"
#include "Scene/SceneParameter.h"
#include "Scene/SceneServices.h"

class Painter;
struct DeviceInput;
enum class TimeDomain { Real, Game };

// A screen of the game: keeps its simulation and views as value members and calls them from its hooks.
// One type for the game and the editor: the game hosts it through Game::SetScene, the editor's catalog opens the same
// class (SceneDefinition::create). Its constructor takes `const SceneServices&` plus its own values.
// The host (Game::Run, the editor's worker) shuts a scene down while it is alive, so a scene needs no destructor.
class Scene {
public:
    // Update runs on game time by default, so hitstop, slow motion and pause reach it; a screen that must keep moving
    // through them (a menu over a paused game) asks for TimeDomain::Real. UpdateReal runs on real time either way.
    explicit Scene(TimeDomain time = TimeDomain::Game);
    virtual ~Scene();
    [[nodiscard]] TimeDomain Clock() const { return _time; }

    // Host side: Game and the editor's worker drive a scene through these; scene code never calls them.
    // Activate begins a visit (Start the first time, then Enter); Deactivate ends it (InputCancelled, Exit).
    void Activate();
    void Deactivate();
    void ReadInput(const DeviceInput& input);
    // Host-adjusted real time, before pause/scaled game time is evaluated.
    void TickReal(float realSeconds);
    void Tick(float deltaSeconds);
    void Render(Painter& paint) const;
    // Cancellation may be requested from an input callback; lifecycle dispatch remains non-reentrant.
    void CancelInput();
    // Ends the visit and runs Stop once. Every step runs before the first failure is rethrown. Idempotent.
    void Shutdown();
    [[nodiscard]] bool IsShutdown() const { return _shutdown; }
    // Evaluated after UpdateReal; application-level pause can independently freeze game time.
    [[nodiscard]] virtual bool GameTimePaused() const;

    // Editor hooks; the game never calls them. Called for Immediate/NextAction values. NextAction setters stage values
    // for the next action. Return an explanation to reject a change; rejected setters must leave content unchanged.
    [[nodiscard]] virtual std::string ApplyParameter(const std::string& id, const SceneValue& value);
    [[nodiscard]] virtual std::string ExecuteAction(const std::string& id);
    // Report authored controls changed by an action or content interaction; omit transient animation state.
    [[nodiscard]] virtual SceneParameters CaptureParameters() const;
    [[nodiscard]] virtual SceneInspection Inspect() const;

protected:
    // Once, before the first Enter.
    virtual void Start();
    // At the start of every visit (each activation, a restart included).
    virtual void Enter();
    // Real time: runs in a hitstop and a pause.
    virtual void UpdateReal(float realSeconds);
    // The scene's clock (TimeDomain); skipped when no time passed.
    virtual void Update(float deltaSeconds);
    virtual void Draw(Painter& paint) const;
    virtual void HandleInput(const DeviceInput& input);
    // Clears held actions and gestures; may repeat, so it must be idempotent.
    virtual void InputCancelled();
    // At the end of every visit, after InputCancelled.
    virtual void Exit();
    // Once, at shutdown, when Start ran.
    virtual void Stop();

private:
    void CheckDispatch() const;
    [[nodiscard]] bool EnsureEntered();
    void CancelInputNow();
    void DeactivateNow();

    TimeDomain _time;
    bool _active = false;
    bool _started = false;
    bool _entered = false;
    bool _shutdown = false;
    mutable bool _dispatching = false;
    bool _cancelling = false;

public:
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) = delete;
    Scene& operator=(Scene&&) = delete;
};
