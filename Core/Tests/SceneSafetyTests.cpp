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
class SceneSafetyProbe final : public Scene {
public:
    explicit SceneSafetyProbe(std::vector<std::string>& log)
        : _log(log) {}
    std::function<void()> duringStart;
    std::function<void()> duringEnter;
    std::function<void()> duringInput;
    std::function<void()> duringCancel;
    std::function<void()> duringUpdate;
    std::function<void()> duringDraw;
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
        if (duringUpdate) duringUpdate();
    }
    void Draw(Painter&) const override {
        _log.push_back("scene.draw");
        if (duringDraw) duringDraw();
    }
    void Stop() override {
        _log.push_back("scene.stop");
        if (duringStop) duringStop();
    }
    std::vector<std::string>& _log;
};
} // namespace

TEST_CASE("Engine.Scene.HooksCannotReenterLifecycleDispatch", "[engine][lifecycle]") {
    std::vector<std::string> log;
    SceneSafetyProbe scene(log);
    int checks = 0;
    const auto checkBlocked = [&] {
        CHECK_THROWS_AS(scene.Activate(), std::logic_error);
        CHECK_THROWS_AS(scene.Deactivate(), std::logic_error);
        CHECK_THROWS_AS(scene.ReadInput({}), std::logic_error);
        CHECK_THROWS_AS(scene.TickReal(0.1f), std::logic_error);
        CHECK_THROWS_AS(scene.Tick(0.1f), std::logic_error);
        CHECK_THROWS_AS(PaintFrame([&](Painter& paint) { scene.Render(paint); }), std::logic_error);
        CHECK_THROWS_AS(scene.Shutdown(), std::logic_error);
        ++checks;
    };

    SECTION("Start") {
        scene.duringStart = checkBlocked;
        CHECK_NOTHROW(scene.Activate());
    }
    SECTION("Enter") {
        scene.duringEnter = checkBlocked;
        CHECK_NOTHROW(scene.Activate());
    }
    SECTION("Update") {
        scene.Activate();
        scene.duringUpdate = checkBlocked;
        CHECK_NOTHROW(scene.Tick(0.1f));
    }
    SECTION("Draw") {
        scene.Activate();
        scene.duringDraw = checkBlocked;
        CHECK_NOTHROW(PaintFrame([&](Painter& paint) { scene.Render(paint); }));
    }
    SECTION("Exit") {
        scene.Activate();
        scene.duringExit = checkBlocked;
        CHECK_NOTHROW(scene.Deactivate());
    }
    SECTION("Stop") {
        scene.Activate();
        scene.duringStop = checkBlocked;
        CHECK_NOTHROW(scene.Shutdown());
    }
    CHECK(checks == 1);
    scene.duringStop = {};
    CHECK_NOTHROW(scene.Shutdown());
    CHECK(scene.IsShutdown());
}

TEST_CASE("Engine.Scene.HookFailureReleasesTheDispatchGuard", "[engine][lifecycle]") {
    std::vector<std::string> log;
    SceneSafetyProbe scene(log);
    const auto fail = [] { throw std::runtime_error("hook failure"); };

    SECTION("Start is not retried; Enter follows on the next dispatch") {
        scene.duringStart = fail;
        CHECK_THROWS_AS(scene.Activate(), std::runtime_error);
        CHECK_NOTHROW(scene.Tick(0.1f));
        CHECK(log == std::vector<std::string>{"scene.start", "scene.enter", "scene.update"});
    }
    SECTION("Enter is not retried; the visit counts as entered") {
        scene.duringEnter = fail;
        CHECK_THROWS_AS(scene.Activate(), std::runtime_error);
        CHECK_NOTHROW(scene.Tick(0.1f));
        CHECK(log == std::vector<std::string>{"scene.start", "scene.enter", "scene.update"});
    }
    SECTION("Update") {
        scene.Activate();
        scene.duringUpdate = fail;
        CHECK_THROWS_AS(scene.Tick(0.1f), std::runtime_error);
        scene.duringUpdate = {};
        CHECK_NOTHROW(scene.Tick(0.1f));
    }
    SECTION("Draw") {
        scene.Activate();
        scene.duringDraw = fail;
        CHECK_THROWS_AS(PaintFrame([&](Painter& paint) { scene.Render(paint); }), std::runtime_error);
        scene.duringDraw = {};
        CHECK_NOTHROW(PaintFrame([&](Painter& paint) { scene.Render(paint); }));
    }
    SECTION("Input") {
        scene.Activate();
        scene.duringInput = fail;
        CHECK_THROWS_AS(scene.ReadInput({}), std::runtime_error);
        scene.duringInput = {};
        CHECK_NOTHROW(scene.ReadInput({}));
    }
    log.clear();
    CHECK_NOTHROW(scene.Shutdown());
    CHECK(log == std::vector<std::string>{"scene.cancel", "scene.exit", "scene.stop"}); // Paired with what began
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
    scene.Shutdown();
}

TEST_CASE("Engine.Scene.ShutdownCompletesCleanupBeforeRethrowingFirstFailure", "[engine][lifecycle]") {
    std::vector<std::string> log;
    SceneSafetyProbe scene(log);
    scene.Activate();
    log.clear();
    scene.duringCancel = [] { throw std::runtime_error("cancel failed"); };
    scene.duringExit = [] { throw std::runtime_error("exit failed"); };
    scene.duringStop = [] { throw std::runtime_error("scene failed"); };
    CHECK_THROWS_WITH(scene.Shutdown(), "cancel failed");
    CHECK(log == std::vector<std::string>{"scene.cancel", "scene.exit", "scene.stop"});
    CHECK(scene.IsShutdown());
    CHECK_NOTHROW(scene.Shutdown());
    CHECK_THROWS_AS(scene.Activate(), std::logic_error);
    CHECK(log.size() == 3);
}

TEST_CASE("Engine.Scene.ShutdownOfAnInactiveSceneStopsWithoutExit", "[engine][lifecycle]") {
    std::vector<std::string> log;
    SceneSafetyProbe scene(log);
    scene.Activate();
    scene.Deactivate();
    log.clear();
    scene.Shutdown();
    CHECK(log == std::vector<std::string>{"scene.stop"});
}
