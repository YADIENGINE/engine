#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "Scene/Scene.h"
#include "Scene/Internals/SceneManager.h"

namespace {
class ManagedScene final : public Scene {
public:
    ManagedScene(std::vector<std::string>& log, std::string name)
        : _log(log)
        , _name(std::move(name)) {}
    std::function<void()> duringEnter;
    std::function<void()> duringExit;
    std::function<void()> duringStop;

private:
    void Start() override { _log.push_back(_name + ".start"); }
    void Enter() override {
        _log.push_back(_name + ".enter");
        if (duringEnter) duringEnter();
    }
    void InputCancelled() override { _log.push_back(_name + ".cancel"); }
    void Exit() override {
        _log.push_back(_name + ".exit");
        if (duringExit) duringExit();
    }
    void Stop() override {
        _log.push_back(_name + ".stop");
        if (duringStop) duringStop();
    }
    std::vector<std::string>& _log;
    std::string _name;
};
} // namespace

TEST_CASE("Engine.SceneManager.CommitsChangesAndRestartsWithoutRepeatingStart", "[engine][scene]") {
    std::vector<std::string> log;
    SceneManager scenes;
    ManagedScene first(log, "first"), second(log, "second");
    scenes.Register(first);
    scenes.Register(second);
    const auto cancel = [&] { log.push_back("devices.cancel"); };
    scenes.Request(first);
    CHECK(scenes.Current() == nullptr);
    CHECK(log.empty());
    CHECK(scenes.Commit(cancel));
    CHECK(scenes.Current() == &first);
    CHECK(log == std::vector<std::string>{"devices.cancel", "first.start", "first.enter"});
    log.clear();

    scenes.Request(first);
    CHECK_FALSE(scenes.Commit(cancel));
    CHECK(log.empty());
    scenes.Request(first, true);
    CHECK(scenes.Commit(cancel));
    CHECK(log == std::vector<std::string>{"devices.cancel", "first.cancel", "first.exit", "first.enter"});
    log.clear();

    scenes.Request(second);
    CHECK(scenes.Commit(cancel));
    CHECK(log ==
          std::vector<std::string>{"devices.cancel", "first.cancel", "first.exit", "second.start", "second.enter"});
    CHECK_NOTHROW(scenes.Shutdown());
}

TEST_CASE("Engine.SceneManager.LastRequestWinsAndLifecycleRequestsWaitForAnotherCommit", "[engine][scene]") {
    std::vector<std::string> log;
    SceneManager scenes;
    ManagedScene first(log, "first"), second(log, "second"), third(log, "third");
    scenes.Register(first);
    scenes.Register(second);
    scenes.Register(third);
    scenes.Request(third);
    scenes.Request(first);
    CHECK(scenes.Commit({}));
    CHECK(scenes.Current() == &first);
    CHECK(log == std::vector<std::string>{"first.start", "first.enter"});

    first.duringExit = [&] { scenes.Request(first); };
    second.duringEnter = [&] { scenes.Request(third); };
    scenes.Request(second);
    CHECK(scenes.Commit({}));
    CHECK(scenes.Current() == &second);
    CHECK(scenes.Commit({}));
    CHECK(scenes.Current() == &third);
    CHECK_FALSE(scenes.Commit({}));
    CHECK_NOTHROW(scenes.Shutdown());
}

TEST_CASE("Engine.SceneManager.HostCleanupFollowsExitOnChangesAndShutdown", "[engine][scene]") {
    std::vector<std::string> log;
    SceneManager scenes([&] { log.push_back("cursor.visible"); });
    ManagedScene first(log, "first"), second(log, "second");
    scenes.Register(first);
    scenes.Register(second);
    scenes.Request(first);
    REQUIRE(scenes.Commit({}));
    log.clear();
    scenes.Request(first, true);
    CHECK(scenes.Commit({}));
    CHECK(log == std::vector<std::string>{"first.cancel", "first.exit", "cursor.visible", "first.enter"});
    log.clear();
    scenes.Request(second);
    CHECK(scenes.Commit({}));
    CHECK(log ==
          std::vector<std::string>{"first.cancel", "first.exit", "cursor.visible", "second.start", "second.enter"});
    log.clear();
    second.duringExit = [] { throw std::runtime_error("exit failure"); };
    CHECK_THROWS_WITH(scenes.Shutdown(), "exit failure");
    CHECK(log ==
          std::vector<std::string>{"second.cancel", "second.exit", "cursor.visible", "second.stop", "first.stop"});
}

TEST_CASE("Engine.SceneManager.RejectsInvalidRegistrationAndRecursiveDispatch", "[engine][scene]") {
    std::vector<std::string> log;
    SceneManager scenes;
    ManagedScene first(log, "first"), other(log, "other");
    scenes.Register(first);
    CHECK_THROWS_AS(scenes.Register(first), std::logic_error);
    CHECK_THROWS_AS(scenes.Request(other), std::logic_error);
    CHECK_FALSE(scenes.Commit({}));
    first.duringEnter = [&] {
        CHECK_THROWS_AS(scenes.Commit({}), std::logic_error);
        CHECK_THROWS_AS(scenes.Shutdown(), std::logic_error);
    };
    scenes.Request(first);
    CHECK(scenes.Commit({}));
    first.duringStop = [&] {
        CHECK_THROWS_AS(scenes.Register(other), std::logic_error);
        CHECK_THROWS_AS(scenes.Request(first), std::logic_error);
    };
    CHECK_NOTHROW(scenes.Shutdown());
    CHECK(scenes.Current() == nullptr);
    CHECK_THROWS_AS(scenes.Register(other), std::logic_error);
    CHECK_THROWS_AS(scenes.Request(first), std::logic_error);
    CHECK_NOTHROW(scenes.Shutdown());
}

TEST_CASE("Engine.SceneManager.ShutdownFinishesAllScenesBeforeRethrowingFirstFailure", "[engine][scene]") {
    std::vector<std::string> log;
    SceneManager scenes;
    ManagedScene first(log, "first"), second(log, "second");
    scenes.Register(first);
    scenes.Register(second);
    scenes.Request(first);
    CHECK(scenes.Commit({}));
    scenes.Request(second);
    CHECK(scenes.Commit({}));
    log.clear();
    second.duringExit = [] { throw std::runtime_error("exit failure"); };
    second.duringStop = [] { throw std::runtime_error("second stop failure"); };
    first.duringStop = [] { throw std::runtime_error("first stop failure"); };
    CHECK_THROWS_WITH(scenes.Shutdown(), "exit failure");
    CHECK(log == std::vector<std::string>{"second.cancel", "second.exit", "second.stop", "first.stop"});
    CHECK(first.IsShutdown());
    CHECK(second.IsShutdown());
    CHECK(scenes.Current() == nullptr);
    CHECK_NOTHROW(scenes.Shutdown());
    CHECK_FALSE(scenes.Commit({}));
}

TEST_CASE("Engine.SceneManager.ActivationFailureCanBeCleanedUpAndPendingShutdownIsRejected", "[engine][scene]") {
    std::vector<std::string> log;
    SceneManager scenes;
    ManagedScene scene(log, "scene");
    scenes.Register(scene);
    scenes.Request(scene);
    SECTION("Enter fails after activation begins") {
        scene.duringEnter = [] { throw std::runtime_error("enter failure"); };
        CHECK_THROWS_WITH(scenes.Commit({}), "enter failure");
        CHECK(scenes.Current() == &scene);
        CHECK_NOTHROW(scenes.Shutdown());
        CHECK(scene.IsShutdown());
        CHECK(scenes.Current() == nullptr);
    }
    SECTION("Pending scene is shut down before commit") {
        scene.Shutdown();
        bool cancelled = false;
        CHECK_THROWS_AS(scenes.Commit([&] { cancelled = true; }), std::logic_error);
        CHECK_FALSE(cancelled);
        CHECK_THROWS_AS(scenes.Request(scene), std::logic_error);
        CHECK_NOTHROW(scenes.Shutdown());
    }
}

namespace {
// Game's shape: the base owns the scene manager, the derived game owns its scenes as value members.
class GameShape {
public:
    GameShape()
        : _scenes(std::make_unique<SceneManager>()) {}
    virtual ~GameShape() = default; // Like ~Game: never touches the borrowed scenes

protected:
    SceneManager& Scenes() { return *_scenes; }

private:
    std::unique_ptr<SceneManager> _scenes;
};

class ThrowingGame final : public GameShape {
public:
    explicit ThrowingGame(std::vector<std::string>& log)
        : _title(log, "title")
        , _play(log, "play") {
        Scenes().Register(_title);
        Scenes().Register(_play);
        Scenes().Request(_title);
        throw std::runtime_error("derived game failed");
    }

private:
    ManagedScene _title;
    ManagedScene _play;
};
} // namespace

TEST_CASE("Engine.SceneManager.AGameConstructorThrowingAfterItsScenesExistLeavesNothingToUndo", "[engine][scene]") {
    // Game's constructor never commits a scene (Run does), so the scenes die before any hook ran, and their
    // destruction needs no shutdown. Debug builds assert in ~Scene if a started scene was not shut down.
    std::vector<std::string> log;
    CHECK_THROWS_WITH(ThrowingGame(log), "derived game failed");
    CHECK(log.empty());
}

TEST_CASE("Engine.SceneManager.ShutdownRunsEveryStartedScenesHooksWhileTheyAreAlive", "[engine][scene]") {
    std::vector<std::string> log;
    SceneManager scenes;
    ManagedScene visited(log, "visited"), current(log, "current"), never(log, "never");
    scenes.Register(visited);
    scenes.Register(current);
    scenes.Register(never);
    scenes.Request(visited);
    CHECK(scenes.Commit({}));
    scenes.Request(current);
    CHECK(scenes.Commit({}));
    log.clear();
    scenes.Shutdown(); // Game::Run's last step, and its failure path
    CHECK(log == std::vector<std::string>{"current.cancel", "current.exit", "current.stop", "visited.stop"});
    CHECK(never.IsShutdown());
}
