#pragma once
#include "Runtime/GameObject.h"
#include "Runtime/ObjectList.h"

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
    void Tick(float deltaSeconds);
    void Render() const;
    // Cancellation may be requested from an input callback; lifecycle dispatch remains non-reentrant.
    void CancelInput();
    void Shutdown();
    // Concrete scene and host destructors use this while their members are still alive.
    void ShutdownNoThrow() noexcept;
    [[nodiscard]] bool IsShutdown() const { return _shutdown; }

protected:
    void AddObject(GameObject& object);
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
