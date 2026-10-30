#pragma once
#include <memory>
#include "GameObjects/GameObject.h"
#include "GameObjects/ObjectList.h"

struct DeviceInput;
enum class TimeDomain { Real, Game };

// A scene keeps domain objects/views as value members. Children update before the scene's simulation hook.
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
    void Render() const;
    // Cancellation may be requested from an input callback; lifecycle dispatch remains non-reentrant.
    void CancelInput();
    void Shutdown();
    // Concrete scene and host destructors use this while their members are still alive.
    void ShutdownNoThrow() noexcept;
    [[nodiscard]] bool IsShutdown() const { return _shutdown; }
    // Evaluated after UpdateReal; application-level pause can independently freeze game time.
    [[nodiscard]] virtual bool GameTimePaused() const;

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
