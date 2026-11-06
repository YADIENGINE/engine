#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <string>
#include <utility>
#include <vector>
#include "Runtime/TimeScale.h"
#include "Scene/TransitionClock.h"
#include "Scene/Scene.h"
#include "Tests/PaintHarness.h"

namespace {
class ProbeScene final : public Scene {
public:
    ProbeScene(std::vector<std::string>& log, std::string name, TimeDomain time = TimeDomain::Real)
        : Scene(time)
        , _log(log)
        , _name(std::move(name)) {}

private:
    void Start() override { _log.push_back(_name + ".start"); }
    void Enter() override { _log.push_back(_name + ".enter"); }
    void Exit() override { _log.push_back(_name + ".exit"); }
    void Update(float seconds) override {
        (void)seconds;
        _log.push_back(_name + ".update");
    }
    void Draw(Painter&) const override { _log.push_back(_name + ".draw"); }
    void Stop() override { _log.push_back(_name + ".stop"); }
    std::vector<std::string>& _log;
    std::string _name;
};
} // namespace

TEST_CASE("Engine.Scene.ReentryKeepsStartAndStopOnce", "[engine]") {
    std::vector<std::string> log;
    ProbeScene scene(log, "scene");
    scene.Activate();
    scene.Tick(0.1f);
    scene.Deactivate();
    scene.Tick(0.1f);
    PaintFrame([&](Painter& paint) { scene.Render(paint); });
    scene.Activate();
    scene.Tick(0.1f);
    scene.Shutdown();
    scene.Shutdown();
    scene.Tick(0.1f);
    PaintFrame([&](Painter& paint) { scene.Render(paint); });
    CHECK(log == std::vector<std::string>{"scene.start", "scene.enter", "scene.update", "scene.exit", "scene.enter",
                                          "scene.update", "scene.exit", "scene.stop"});
}

TEST_CASE("Engine.Scene.NeverActivatedSceneRunsNoHooks", "[engine]") {
    std::vector<std::string> log;
    {
        ProbeScene scene(log, "scene");
        scene.Tick(0.1f);
        PaintFrame([&](Painter& paint) { scene.Render(paint); });
        scene.Deactivate();
    } // Nothing started, so nothing is left to undo and the scene needs no shutdown.
    ProbeScene shut(log, "shut");
    shut.Shutdown();
    CHECK(log.empty());
}

TEST_CASE("Engine.Time.ZeroGameTimeKeepsDrawingAndRealTimeScenesAdvance", "[engine]") {
    std::vector<std::string> log;
    ProbeScene game(log, "game", TimeDomain::Game);
    ProbeScene real(log, "real");
    game.Activate();
    real.Activate();
    log.clear();
    TimeScale time;
    time.Hitstop(0.5f);
    game.Tick(time.Advance(0.1f));
    real.Tick(0.1f);
    PaintFrame([&](Painter& paint) { game.Render(paint); });
    CHECK(log == std::vector<std::string>{"real.update", "game.draw"});
    game.Shutdown();
    real.Shutdown();
}

TEST_CASE("Engine.Time.HitstopFreezesAndKeepsLongerRequest", "[engine][time]") {
    TimeScale time;
    time.Hitstop(0.5f);
    time.Hitstop(0.1f);
    CHECK(time.Advance(0.25f) == 0.0f);
    CHECK(time.Advance(0.25f) == 0.0f);
    CHECK(time.Advance(0.25f) == 0.25f);
}

TEST_CASE("Engine.Time.FixedSlowMotionHoldsUntilExpiryAndResets", "[engine][time]") {
    TimeScale time;
    constexpr float duration = 1.25f;
    constexpr float scale = 0.25f;
    constexpr float step = 0.25f;
    time.SlowMo(duration, scale, scale);

    SECTION("Hitstop preserves the full slow motion duration") {
        time.Hitstop(step);
        CHECK(time.Advance(step) == 0.0f);
        for (int i = 0; i < 5; ++i)
            CHECK(time.Advance(step) == Catch::Approx(step * scale));
        CHECK(time.Advance(step) == step);
    }

    SECTION("Clear restores normal time") {
        time.Clear();
        CHECK(time.Advance(step) == step);
    }

    SECTION("A later default slow motion recovers normally") {
        time.SlowMo(duration, scale);
        CHECK(time.Advance(duration * 0.5f) == Catch::Approx(duration * 0.5f * scale));
        CHECK(time.Advance(duration * 0.5f) == Catch::Approx(duration * 0.5f * (scale + 1.0f) * 0.5f));
        CHECK(time.Advance(step) == step);
    }
}

TEST_CASE("Engine.Transition.MidpointIsEmittedOnceAndZeroTimeDoesNotAdvance", "[engine]") {
    TransitionClock transition(0.5f, 0.1f, 0.5f);
    transition.Start();
    CHECK_FALSE(transition.Update(0.0f));
    CHECK_FALSE(transition.Update(0.4f));
    CHECK(transition.Update(0.15f));
    CHECK_FALSE(transition.Update(0.1f));
    CHECK(transition.Cover() < 1.0f);
    CHECK(transition.Opening());
    CHECK_FALSE(transition.Update(1.0f));
    CHECK_FALSE(transition.Active());
}
