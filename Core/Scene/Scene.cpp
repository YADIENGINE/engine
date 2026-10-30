#include "Scene/Scene.h"
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <utility>
#include "Input/Input.hpp"

Scene::Scene(TimeDomain time)
    : _time(time) {
    _self.Add(*this);
}
void Scene::AddObject(GameObject& object) { _objects.Add(object); }
GameObject& Scene::AddObject(std::unique_ptr<GameObject> object) { return _objects.Add(std::move(object)); }
void Scene::RemoveObject(GameObject& object) { _objects.Remove(object); }
void Scene::ClearObjects() { _objects.Clear(); }
void Scene::UpdateReal(float realSeconds) { (void)realSeconds; }
bool Scene::GameTimePaused() const { return false; }
void Scene::Enter() {}
void Scene::Exit() {}
void Scene::HandleInput(const DeviceInput& input) { (void)input; }
void Scene::InputCancelled() {}
void Scene::CheckDispatch() const {
    if (_dispatching) throw std::logic_error("Cannot dispatch a scene recursively");
}
bool Scene::EnsureEntered() {
    if (!_active || _shutdown || !Enabled()) return false;
    if (!_entered) {
        _self.Start();
        if (!Enabled()) return false;
        _objects.Start();
        if (!Enabled()) return false;
        _entered = true;
        Enter();
    }
    return Enabled();
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
        if (EnsureEntered()) {
            _objects.Update(deltaSeconds);
            if (Enabled()) _self.Update(deltaSeconds);
        }
    } catch (...) {
        _dispatching = false;
        throw;
    }
    _dispatching = false;
}
void Scene::Render() const {
    CheckDispatch();
    if (!_active || !_entered || !Enabled()) return;
    _dispatching = true;
    try {
        _objects.Draw();
        if (Enabled()) _self.Draw();
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
    try {
        _objects.Stop();
    } catch (...) {
        if (!failure) failure = std::current_exception();
    }
    try {
        _self.Stop();
    } catch (...) {
        if (!failure) failure = std::current_exception();
    }
    _dispatching = false;
    if (failure) std::rethrow_exception(failure);
}
void Scene::ShutdownNoThrow() noexcept {
    try {
        Shutdown();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Scene shutdown failed: %s\n", error.what());
    } catch (...) {
        std::fputs("Scene shutdown failed with an unknown exception\n", stderr);
    }
}
