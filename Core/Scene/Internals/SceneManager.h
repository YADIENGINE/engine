#pragma once
#include <functional>
#include <vector>

class Scene;

// Borrows scenes owned by the application. Shutdown must run while those scenes are still alive.
class SceneManager {
public:
    // Optional host cleanup after deactivation, even if Exit throws; also runs before Stop at shutdown.
    explicit SceneManager(std::function<void()> afterExit = {});
    ~SceneManager();
    void Register(Scene& scene);
    void Request(Scene& scene, bool restart = false);
    // Calls beforeChange only for a real change, before leaving the previous scene.
    [[nodiscard]] bool Commit(const std::function<void()>& beforeChange);
    // Makes a registered scene current at once and leaves a queued request queued: the loading screen, before the first
    // scene. Commit is the way out when a request is waiting; Close when the game may have none.
    void Show(Scene& scene);
    // Leaves the current scene without entering another. Calls beforeChange only when there is one to leave.
    void Close(const std::function<void()>& beforeChange);
    [[nodiscard]] Scene* Current() const { return _current; }
    // Shuts every scene down before rethrowing the first failure. Idempotent.
    void Shutdown();

private:
    void DeactivateCurrent();
    void Switch(Scene* next, const std::function<void()>& beforeChange);

    std::function<void()> _afterExit;
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
