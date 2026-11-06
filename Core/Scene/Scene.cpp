#include "Scene/Scene.h"
#include <cassert>
#include <exception>
#include <stdexcept>
#include <utility>
#include "Input/Input.h"

Scene::Scene(TimeDomain time)
    : _time(time) {}
Scene::~Scene() {
    // Exit and Stop need the derived scene, which is gone here: its host shuts it down first.
    assert((!_started || _shutdown) && "A started scene is shut down by its host before it is destroyed");
}
void Scene::Start() {}
void Scene::Enter() {}
void Scene::UpdateReal(float realSeconds) { (void)realSeconds; }
void Scene::Update(float deltaSeconds) { (void)deltaSeconds; }
void Scene::Draw(Painter& paint) const { (void)paint; }
void Scene::HandleInput(const DeviceInput& input) { (void)input; }
void Scene::InputCancelled() {}
void Scene::Exit() {}
void Scene::Stop() {}
bool Scene::GameTimePaused() const { return false; }
std::string Scene::ApplyParameter(const std::string& id, const SceneValue& value) {
    (void)value;
    return "Unsupported parameter: " + id;
}
std::string Scene::ExecuteAction(const std::string& id) { return "Unsupported action: " + id; }
SceneParameters Scene::CaptureParameters() const { return {}; }
SceneInspection Scene::Inspect() const { return {}; }
void Scene::CheckDispatch() const {
    if (_dispatching) throw std::logic_error("Cannot dispatch a scene recursively");
}
bool Scene::EnsureEntered() {
    if (!_active || _shutdown) return false;
    // Flags first: a hook that throws is not retried, and Stop/Exit still pair with it at shutdown.
    if (!_started) {
        _started = true;
        Start();
    }
    if (!_entered) {
        _entered = true;
        Enter();
    }
    return true;
}
void Scene::Activate() {
    CheckDispatch();
    if (_shutdown) throw std::logic_error("Cannot activate a scene after shutdown");
    if (_active) return;
    _active = true;
    _dispatching = true;
    try {
        (void)EnsureEntered();
    } catch (...) {
        _dispatching = false;
        throw;
    }
    _dispatching = false;
}
void Scene::CancelInputNow() {
    if (_cancelling) return;
    _cancelling = true;
    try {
        InputCancelled();
    } catch (...) {
        _cancelling = false;
        throw;
    }
    _cancelling = false;
}
void Scene::CancelInput() {
    if (_shutdown || _cancelling) return;
    const bool wasDispatching = std::exchange(_dispatching, true);
    try {
        CancelInputNow();
    } catch (...) {
        _dispatching = wasDispatching;
        throw;
    }
    _dispatching = wasDispatching;
}
void Scene::DeactivateNow() {
    if (!_active) return;
    _active = false;
    const bool entered = std::exchange(_entered, false);
    std::exception_ptr failure;
    try {
        CancelInputNow();
    } catch (...) {
        failure = std::current_exception();
    }
    if (entered) {
        try {
            Exit();
        } catch (...) {
            if (!failure) failure = std::current_exception();
        }
    }
    if (failure) std::rethrow_exception(failure);
}
void Scene::Deactivate() {
    CheckDispatch();
    _dispatching = true;
    try {
        DeactivateNow();
    } catch (...) {
        _dispatching = false;
        throw;
    }
    _dispatching = false;
}
void Scene::ReadInput(const DeviceInput& input) {
    CheckDispatch();
    if (!_active || _shutdown) return;
    _dispatching = true;
    try {
        if (input.cancelled)
            CancelInputNow();
        else if (EnsureEntered())
            HandleInput(input);
    } catch (...) {
        _dispatching = false;
        throw;
    }
    _dispatching = false;
}
void Scene::TickReal(float realSeconds) {
    CheckDispatch();
    _dispatching = true;
    try {
        if (EnsureEntered() && realSeconds > 0.0f) UpdateReal(realSeconds);
    } catch (...) {
        _dispatching = false;
        throw;
    }
    _dispatching = false;
}
void Scene::Tick(float deltaSeconds) {
    CheckDispatch();
    _dispatching = true;
    try {
        if (EnsureEntered() && deltaSeconds > 0.0f) Update(deltaSeconds);
    } catch (...) {
        _dispatching = false;
        throw;
    }
    _dispatching = false;
}
void Scene::Render(Painter& paint) const {
    CheckDispatch();
    if (!_active || !_entered) return;
    _dispatching = true;
    try {
        Draw(paint);
    } catch (...) {
        _dispatching = false;
        throw;
    }
    _dispatching = false;
}
void Scene::Shutdown() {
    CheckDispatch();
    if (_shutdown) return;
    _shutdown = true;
    _dispatching = true;
    std::exception_ptr failure;
    try {
        DeactivateNow();
    } catch (...) {
        failure = std::current_exception();
    }
    if (_started) {
        try {
            Stop();
        } catch (...) {
            if (!failure) failure = std::current_exception();
        }
    }
    _dispatching = false;
    if (failure) std::rethrow_exception(failure);
}
