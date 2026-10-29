#pragma once
#include <functional>
#include <vector>

class Scene;

// Borrows scenes owned by the application. Shutdown must run while those scenes are still alive.
class SceneManager {
public:
    SceneManager();
    ~SceneManager();
    void Register(Scene& scene);
    void Request(Scene& scene, bool restart = false);
    // Calls beforeChange only for a real change, before leaving the previous scene.
    [[nodiscard]] bool Commit(const std::function<void()>& beforeChange);
    [[nodiscard]] Scene* Current() const { return _current; }
    // Shuts every scene down before rethrowing the first failure. Idempotent.
    void Shutdown();

private:
    std::vector<Scene*> _scenes;
    Scene* _current = nullptr;
    Scene* _pending = nullptr;
    bool _restart = false;
    bool _dispatching = false;
    bool _shutdown = false;

public:
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;
    SceneManager(SceneManager&&) = delete;
    SceneManager& operator=(SceneManager&&) = delete;
};
