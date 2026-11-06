#pragma once
#include <memory>
#include <string>
#include "GameObjects/GameObject.h"
#include "GameObjects/ObjectList.h"
#include "Scene/SceneInspection.h"
#include "Scene/SceneParameter.h"
#include "Scene/SceneServices.h"

struct DeviceInput;
enum class TimeDomain { Real, Game };

// A scene keeps domain objects/views as value members. Children update before the scene's simulation hook.
// One type for the game and the editor: the game hosts it through Game::SetScene, the editor's catalog opens the same
// class (SceneDefinition::create). Its constructor takes `const SceneServices&` plus its own values.
class Scene : public GameObject {
public:
    explicit Scene(TimeDomain time = TimeDomain::Real);
    [[nodiscard]] TimeDomain Clock() const { return _time; }
    void Activate();
    void Deactivate();
    void ReadInput(const DeviceInput& input);
    // Host-adjusted real time, before pause/scaled game time is evaluated. Does not update children.
    void TickReal(float realSeconds);
    void Tick(float deltaSeconds);
    void Render(Painter& paint) const;
    // Cancellation may be requested from an input callback; lifecycle dispatch remains non-reentrant.
    void CancelInput();
    void Shutdown();
    // Concrete scene and host destructors use this while their members are still alive.
    void ShutdownNoThrow() noexcept;
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
    void AddObject(GameObject& object);
    GameObject& AddObject(std::unique_ptr<GameObject> object);
    // Removal suppresses callbacks immediately; Stop/destruction wait for a dispatch or shutdown.
    void RemoveObject(GameObject& object);
    void ClearObjects();
    virtual void UpdateReal(float realSeconds);
    virtual void Enter();
    virtual void Exit();
    virtual void HandleInput(const DeviceInput& input);
    virtual void InputCancelled();

private:
    void CheckDispatch() const;
    [[nodiscard]] bool EnsureEntered();
    void CancelInputNow();
    void DeactivateNow();

    TimeDomain _time;
    ObjectList _self;
    ObjectList _objects;
    bool _active = false;
    bool _entered = false;
    bool _shutdown = false;
    mutable bool _dispatching = false;
    bool _cancelling = false;
};
