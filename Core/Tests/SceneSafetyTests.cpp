#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
#include "Input/Input.h"
#include "Scene/Scene.h"
#include "Tests/PaintHarness.h"

namespace {
class SceneSafetyChild final : public GameObject {
public:
    explicit SceneSafetyChild(std::vector<std::string>& log)
        : _log(log) {}
    std::function<void()> duringStart;
    std::function<void()> duringUpdate;
    std::function<void()> duringStop;

private:
    void Start() override {
        _log.push_back("child.start");
        if (duringStart) duringStart();
    }
    void Update(float seconds) override {
        (void)seconds;
        _log.push_back("child.update");
        if (duringUpdate) duringUpdate();
    }
    void Stop() override {
        _log.push_back("child.stop");
        if (duringStop) duringStop();
    }
    std::vector<std::string>& _log;
};

class SceneSafetyProbe final : public Scene {
public:
    explicit SceneSafetyProbe(std::vector<std::string>& log)
        : _log(log)
        , _child(log) {
        AddObject(_child);
    }
    ~SceneSafetyProbe() override { ShutdownNoThrow(); }
    [[nodiscard]] SceneSafetyChild& Child() { return _child; }
    std::function<void()> duringStart;
    std::function<void()> duringEnter;
    std::function<void()> duringInput;
    std::function<void()> duringCancel;
    std::function<void()> duringExit;
    std::function<void()> duringStop;

private:
    void Start() override {
        _log.push_back("scene.start");
        if (duringStart) duringStart();
    }
    void Enter() override {
        _log.push_back("scene.enter");
        if (duringEnter) duringEnter();
    }
    void HandleInput(const DeviceInput& input) override {
        (void)input;
        _log.push_back("scene.input");
        if (duringInput) duringInput();
    }
    void InputCancelled() override {
        _log.push_back("scene.cancel");
        if (duringCancel) duringCancel();
    }
    void Exit() override {
        _log.push_back("scene.exit");
        if (duringExit) duringExit();
    }
    void Update(float seconds) override {
        (void)seconds;
        _log.push_back("scene.update");
    }
    void Draw(Painter&) const override { _log.push_back("scene.draw"); }
    void Stop() override {
        _log.push_back("scene.stop");
        if (duringStop) duringStop();
    }
    std::vector<std::string>& _log;
    SceneSafetyChild _child;
};
} // namespace

TEST_CASE("Engine.Scene.DisabledActivationDefersInitializationBeforeInput", "[engine][lifecycle]") {
    std::vector<std::string> log;
    SceneSafetyProbe scene(log);
    scene.SetEnabled(false);
    scene.Activate();
    scene.Tick(0.1f);
    PaintFrame([&](Painter& paint) { scene.Render(paint); });
    CHECK(log.empty());
    scene.SetEnabled(true);
    scene.ReadInput({});
    CHECK(log == std::vector<std::string>{"scene.start", "child.start", "scene.enter", "scene.input"});
}

TEST_CASE("Engine.Scene.DisablingDuringInitializationDefersRemainingCallbacks", "[engine][lifecycle]") {
    std::vector<std::string> log;
    SceneSafetyProbe scene(log);
    SECTION("Scene Start disables the scene") {
        scene.duringStart = [&] { scene.SetEnabled(false); };
        scene.Activate();
        CHECK(log == std::vector<std::string>{"scene.start"});
    }
    SECTION("Child Start disables the scene") {
        scene.Child().duringStart = [&] { scene.SetEnabled(false); };
        scene.Activate();
        CHECK(log == std::vector<std::string>{"scene.start", "child.start"});
    }
    SECTION("Enter disables the scene") {
        scene.duringEnter = [&] { scene.SetEnabled(false); };
        scene.Activate();
        CHECK(log == std::vector<std::string>{"scene.start", "child.start", "scene.enter"});
    }
    scene.SetEnabled(true);
    scene.ReadInput({});
    CHECK(log == std::vector<std::string>{"scene.start", "child.start", "scene.enter", "scene.input"});
}

TEST_CASE("Engine.Scene.ChildCallbacksCannotReenterLifecycleDispatch", "[engine][lifecycle]") {
    std::vector<std::string> log;
    SceneSafetyProbe scene(log);
    scene.Activate();
    scene.Child().duringUpdate = [&] { scene.Deactivate(); };
    CHECK_THROWS_AS(scene.Tick(0.1f), std::logic_error);
    CHECK_FALSE(scene.IsShutdown());
    scene.Child().duringUpdate = {};
    log.clear();
    CHECK_NOTHROW(scene.Tick(0.1f));
    CHECK(log == std::vector<std::string>{"child.update", "scene.update"});
}

TEST_CASE("Engine.Scene.CancellationConsumesInputAndAllowsNestedCancellation", "[engine][lifecycle][input]") {
    std::vector<std::string> log;
    SceneSafetyProbe scene(log);
    scene.Activate();
    log.clear();
    SECTION("A cancelled snapshot is not forwarded as an action") {
        DeviceInput input;
        input.cancelled = true;
        input.keysPressed[static_cast<std::size_t>(InputKey::Enter)] = true;
        scene.ReadInput(input);
        CHECK(log == std::vector<std::string>{"scene.cancel"});
    }
    SECTION("Input callbacks may cancel and recursive cancellation is idempotent") {
        scene.duringInput = [&] { scene.CancelInput(); };
        scene.duringCancel = [&] {
            scene.CancelInput();
            CHECK_THROWS_AS(scene.Deactivate(), std::logic_error);
        };
        scene.ReadInput({});
        CHECK(log == std::vector<std::string>{"scene.input", "scene.cancel"});
        scene.duringCancel = {};
    }
}

TEST_CASE("Engine.Scene.ShutdownCompletesCleanupBeforeRethrowingFirstFailure", "[engine][lifecycle]") {
    std::vector<std::string> log;
    SceneSafetyProbe scene(log);
    scene.Activate();
    log.clear();
    scene.duringCancel = [] { throw std::runtime_error("cancel failed"); };
    scene.duringExit = [] { throw std::runtime_error("exit failed"); };
    scene.Child().duringStop = [] { throw std::runtime_error("child failed"); };
    scene.duringStop = [] { throw std::runtime_error("scene failed"); };
    CHECK_THROWS_WITH(scene.Shutdown(), "cancel failed");
    CHECK(log == std::vector<std::string>{"scene.cancel", "scene.exit", "child.stop", "scene.stop"});
    CHECK(scene.IsShutdown());
    CHECK_NOTHROW(scene.Shutdown());
    CHECK_THROWS_AS(scene.Activate(), std::logic_error);
}

TEST_CASE("Engine.Scene.DestructorCleanupDoesNotPropagateCallbackFailures", "[engine][lifecycle]") {
    std::vector<std::string> log;
    {
        SceneSafetyProbe scene(log);
        scene.Activate();
        log.clear();
        scene.duringExit = [] { throw std::runtime_error("exit failed"); };
        scene.Child().duringStop = [] { throw std::runtime_error("child failed"); };
    }
    CHECK(log == std::vector<std::string>{"scene.cancel", "scene.exit", "child.stop", "scene.stop"});
}
