#include "Scene/Internals/SceneManager.h"
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>
#include "Scene/Scene.h"

SceneManager::SceneManager() = default;
// Scene references may already be gone when the application's base Game is destroyed.
SceneManager::~SceneManager() = default;

void SceneManager::Register(Scene& scene) {
    if (_shutdown) throw std::logic_error("Cannot register scenes during shutdown");
    if (std::ranges::find(_scenes, &scene) != _scenes.end()) throw std::logic_error("Scene already registered");
    if (scene.IsShutdown()) throw std::logic_error("Cannot register a scene after shutdown");
    _scenes.push_back(&scene);
}

void SceneManager::Request(Scene& scene, bool restart) {
    if (_shutdown) throw std::logic_error("Cannot select scenes during shutdown");
    if (std::ranges::find(_scenes, &scene) == _scenes.end()) throw std::logic_error("Scene is not registered");
    if (scene.IsShutdown()) throw std::logic_error("Cannot select a scene after shutdown");
    _pending = &scene;
    _restart = restart;
}

bool SceneManager::Commit(const std::function<void()>& beforeChange) {
    if (_dispatching) throw std::logic_error("Cannot dispatch scene changes recursively");
    if (_shutdown || !_pending) return false;
    Scene* next = std::exchange(_pending, nullptr);
    const bool restart = std::exchange(_restart, false);
    if (next->IsShutdown()) throw std::logic_error("Cannot activate a scene after shutdown");
    if (next == _current && !restart) return false;
    _dispatching = true;
    try {
        if (beforeChange) beforeChange();
        if (_current) _current->Deactivate();
        _current = next;
        _current->Activate();
    } catch (...) {
        _dispatching = false;
        throw;
    }
    _dispatching = false;
    return true;
}

void SceneManager::Shutdown() {
    if (_dispatching) throw std::logic_error("Cannot shut down scenes during dispatch");
    if (_shutdown) return;
    _shutdown = true;
    _dispatching = true;
    std::exception_ptr failure;
    for (auto scene = _scenes.rbegin(); scene != _scenes.rend(); ++scene) {
        try {
            (*scene)->Shutdown();
        } catch (...) {
            if (!failure) failure = std::current_exception();
        }
    }
    _current = nullptr;
    _pending = nullptr;
    _restart = false;
    _scenes.clear();
    _dispatching = false;
    if (failure) std::rethrow_exception(failure);
}
