#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "Fx/TimeScale.h"
#include "Fx/TransitionClock.h"
#include "GameObjects/ObjectList.h"
#include "Scene/Scene.h"
#include "Tests/PaintHarness.h"

namespace {
class Probe final : public GameObject {
public:
    Probe(std::vector<std::string>& log, std::string name)
        : _log(log)
        , _name(std::move(name)) {}
    ~Probe() override { _log.push_back(_name + ".destroy"); }
    std::function<void()> duringUpdate;

private:
    void Start() override { _log.push_back(_name + ".start"); }
    void Update(float seconds) override {
        (void)seconds;
        _log.push_back(_name + ".update");
        if (duringUpdate) duringUpdate();
    }
    void Draw(Painter&) const override { _log.push_back(_name + ".draw"); }
    void Stop() override { _log.push_back(_name + ".stop"); }
    std::vector<std::string>& _log;
    std::string _name;
};

class ProbeScene final : public Scene {
public:
    explicit ProbeScene(std::vector<std::string>& log)
        : _log(log)
        , _child(log, "child") {
        AddObject(_child);
    }
    ~ProbeScene() override { ShutdownNoThrow(); }

private:
    void Start() override { _log.push_back("scene.start"); }
    void Enter() override { _log.push_back("scene.enter"); }
    void Exit() override { _log.push_back("scene.exit"); }
    void Update(float seconds) override {
        (void)seconds;
        _log.push_back("scene.update");
    }
    void Stop() override { _log.push_back("scene.stop"); }
    std::vector<std::string>& _log;
    Probe _child;
};
} // namespace

TEST_CASE("Engine.Lifecycle.StartsOnceAndDisabledObjectsSkipCallbacks", "[engine]") {
    std::vector<std::string> log;
    Probe object(log, "object");
    ObjectList objects;
    object.SetEnabled(false);
    objects.Add(object);
    ObjectList other;
    CHECK_THROWS_AS(other.Add(object), std::logic_error);
    objects.Start();
    objects.Update(0.1f);
    PaintFrame([&](Painter& paint) { objects.Draw(paint); });
    CHECK(log.empty());
    object.SetEnabled(true);
    objects.Update(0.1f);
    PaintFrame([&](Painter& paint) { objects.Draw(paint); });
    object.SetEnabled(false);
    objects.Update(0.1f);
    object.SetEnabled(true);
    objects.Update(0.1f);
    objects.Stop();
    objects.Stop();
    objects.Update(0.1f);
    PaintFrame([&](Painter& paint) { objects.Draw(paint); });
    CHECK(log ==
          std::vector<std::string>{"object.start", "object.update", "object.draw", "object.update", "object.stop"});
    CHECK_THROWS_AS(other.Add(object), std::logic_error);
}

TEST_CASE("Engine.Lifecycle.RemovalStopsOwnedObjectsBeforeDestruction", "[engine]") {
    std::vector<std::string> log;
    ObjectList objects;
    auto owned = std::make_unique<Probe>(log, "owned");
    Probe& object = *owned;
    object.duringUpdate = [&] { objects.Remove(object); };
    objects.Add(std::move(owned));
    objects.Update(0.1f);
    PaintFrame([&](Painter& paint) { objects.Draw(paint); });
    CHECK(log == std::vector<std::string>{"owned.start", "owned.update"});
    objects.Update(0.1f);
    CHECK(log == std::vector<std::string>{"owned.start", "owned.update", "owned.stop", "owned.destroy"});
}

TEST_CASE("Engine.Lifecycle.EnablingDuringUpdateStartsBeforeTheNextUpdate", "[engine]") {
    std::vector<std::string> log;
    Probe first(log, "first");
    Probe second(log, "second");
    ObjectList objects;
    second.SetEnabled(false);
    first.duringUpdate = [&] { second.SetEnabled(true); };
    objects.Add(first);
    objects.Add(second);
    objects.Update(0.1f);
    PaintFrame([&](Painter& paint) { objects.Draw(paint); });
    CHECK(log == std::vector<std::string>{"first.start", "first.update", "first.draw"});
    log.clear();
    objects.Update(0.1f);
    CHECK(log == std::vector<std::string>{"second.start", "first.update", "second.update"});
}

TEST_CASE("Engine.Lifecycle.NewObjectsStartOnTheNextDispatch", "[engine]") {
    std::vector<std::string> log;
    Probe first(log, "first");
    Probe second(log, "second");
    ObjectList objects;
    bool added = false;
    first.duringUpdate = [&] {
        if (!added) {
            objects.Add(second);
            added = true;
        }
    };
    objects.Add(first);
    objects.Update(0.1f);
    PaintFrame([&](Painter& paint) { objects.Draw(paint); });
    CHECK(log == std::vector<std::string>{"first.start", "first.update", "first.draw"});
    log.clear();
    objects.Update(0.1f);
    CHECK(log == std::vector<std::string>{"second.start", "first.update", "second.update"});
}

TEST_CASE("Engine.Scene.ReentryKeepsStartOnceAndChildrenPrecedeSimulation", "[engine]") {
    std::vector<std::string> log;
    ProbeScene scene(log);
    scene.Activate();
    scene.Tick(0.1f);
    scene.Deactivate();
    scene.Tick(0.1f);
    scene.Activate();
    scene.Tick(0.1f);
    scene.Shutdown();
    CHECK(log == std::vector<std::string>{"scene.start", "child.start", "scene.enter", "child.update", "scene.update",
                                          "scene.exit", "scene.enter", "child.update", "scene.update", "scene.exit",
                                          "child.stop", "scene.stop"});
}

TEST_CASE("Engine.Time.ZeroGameTimeKeepsDrawingAndRealTimeObjectsAdvance", "[engine]") {
    std::vector<std::string> log;
    Probe visual(log, "visual");
    Probe transition(log, "transition");
    ObjectList game;
    ObjectList real;
    game.Add(visual);
    real.Add(transition);
    TimeScale time;
    time.Hitstop(0.5f);
    game.Update(time.Advance(0.1f));
    real.Update(0.1f);
    PaintFrame([&](Painter& paint) { game.Draw(paint); });
    CHECK(log == std::vector<std::string>{"visual.start", "transition.start", "transition.update", "visual.draw"});
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
    bool opening = false;
    CHECK(transition.Cover(opening) < 1.0f);
    CHECK(opening);
    CHECK_FALSE(transition.Update(1.0f));
    CHECK_FALSE(transition.Active());
}
