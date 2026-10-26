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
    void Shutdown();

protected:
    void AddObject(GameObject& object);
    virtual void Enter();
    virtual void Exit();
    virtual void HandleInput(const DeviceInput& input);

private:
    TimeDomain _time;
    ObjectList _self;
    ObjectList _objects;
    bool _active = false;
    bool _shutdown = false;
};
