#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include "GameObjects/ObjectList.h"
#include "Tests/PaintHarness.h"

namespace {
struct ObjectCalls {
    int starts = 0;
    int updates = 0;
    int draws = 0;
    int stops = 0;
    int destroyed = 0;
};

class ObjectListProbe final : public GameObject {
public:
    explicit ObjectListProbe(ObjectCalls& calls)
        : _calls(calls) {}
    ~ObjectListProbe() override { ++_calls.destroyed; }
    std::function<void()> onStart;
    std::function<void()> onUpdate;
    std::function<void()> onDraw;
    std::function<void()> onStop;

private:
    void Start() override {
        ++_calls.starts;
        if (onStart) onStart();
    }
    void Update(float seconds) override {
        (void)seconds;
        ++_calls.updates;
        if (onUpdate) onUpdate();
    }
    void Draw(Painter&) const override {
        ++_calls.draws;
        if (onDraw) onDraw();
    }
    void Stop() override {
        ++_calls.stops;
        if (onStop) onStop();
    }
    ObjectCalls& _calls;
};
} // namespace

TEST_CASE("Engine.Lifecycle.StopFinishesCleanupBeforeRethrowing", "[engine][lifecycle]") {
    ObjectCalls firstCalls, lastCalls, pendingCalls;
    ObjectListProbe pending(pendingCalls);
    ObjectList objects;
    auto first = std::make_unique<ObjectListProbe>(firstCalls);
    auto last = std::make_unique<ObjectListProbe>(lastCalls);
    first->onStop = [] { throw std::runtime_error("later failure"); };
    last->onStop = [] { throw std::runtime_error("first failure"); };
    objects.Add(std::move(first));
    objects.Add(std::move(last));
    objects.Start();
    objects.Add(pending);

    try {
        objects.Stop();
        FAIL("Stop must report the first callback failure");
    } catch (const std::runtime_error& error) {
        CHECK(std::string(error.what()) == "first failure");
    }
    CHECK(firstCalls.stops == 1);
    CHECK(lastCalls.stops == 1);
    CHECK(firstCalls.destroyed == 1);
    CHECK(lastCalls.destroyed == 1);
    CHECK(pendingCalls.stops == 0);
    CHECK_NOTHROW(objects.Stop());
    ObjectList other;
    CHECK_NOTHROW(other.Add(pending));
}

TEST_CASE("Engine.Lifecycle.DestructorFinishesCleanupWithoutThrowing", "[engine][lifecycle]") {
    ObjectCalls firstCalls, lastCalls;
    {
        ObjectList objects;
        auto first = std::make_unique<ObjectListProbe>(firstCalls);
        auto last = std::make_unique<ObjectListProbe>(lastCalls);
        last->onStop = [] { throw std::runtime_error("destructor cleanup"); };
        objects.Add(std::move(first));
        objects.Add(std::move(last));
        objects.Start();
    }
    CHECK(firstCalls.stops == 1);
    CHECK(lastCalls.stops == 1);
    CHECK(firstCalls.destroyed == 1);
    CHECK(lastCalls.destroyed == 1);
}

TEST_CASE("Engine.Lifecycle.RemovalFailureKeepsTheListUsable", "[engine][lifecycle]") {
    ObjectCalls removedCalls, survivorCalls, pendingCalls;
    ObjectListProbe survivor(survivorCalls), pending(pendingCalls);
    ObjectList objects;
    auto removed = std::make_unique<ObjectListProbe>(removedCalls);
    removed->onStop = [&] {
        objects.Add(pending);
        throw std::runtime_error("removal cleanup");
    };
    GameObject& target = objects.Add(std::move(removed));
    objects.Add(survivor);
    objects.Start();
    objects.Remove(target);

    CHECK_THROWS_AS(objects.Update(0.1f), std::runtime_error);
    CHECK(removedCalls.stops == 1);
    CHECK(removedCalls.destroyed == 1);
    CHECK(survivorCalls.updates == 0);
    CHECK(pendingCalls.starts == 0);
    CHECK_NOTHROW(objects.Update(0.1f));
    CHECK(survivorCalls.updates == 1);
    CHECK(pendingCalls.starts == 1);
    CHECK(pendingCalls.updates == 1);
    CHECK_NOTHROW(objects.Stop());
    CHECK(removedCalls.stops == 1);
    CHECK(survivorCalls.stops == 1);
    CHECK(pendingCalls.stops == 1);
}

TEST_CASE("Engine.Lifecycle.CallbacksRejectRecursiveDispatchAndShutdown", "[engine][lifecycle]") {
    ObjectCalls calls;
    ObjectListProbe object(calls);
    ObjectList objects;
    objects.Add(object);
    const auto checkBlocked = [&] {
        CHECK_THROWS_AS(objects.Start(), std::logic_error);
        CHECK_THROWS_AS(objects.Update(0.1f), std::logic_error);
        CHECK_THROWS_AS(PaintFrame([&](Painter& paint) { objects.Draw(paint); }), std::logic_error);
        CHECK_THROWS_AS(objects.Stop(), std::logic_error);
    };

    SECTION("Start callback") {
        object.onStart = checkBlocked;
        CHECK_NOTHROW(objects.Start());
    }
    SECTION("Update callback") {
        object.onUpdate = checkBlocked;
        CHECK_NOTHROW(objects.Update(0.1f));
    }
    SECTION("Draw callback") {
        objects.Start();
        object.onDraw = checkBlocked;
        CHECK_NOTHROW(PaintFrame([&](Painter& paint) { objects.Draw(paint); }));
    }
    SECTION("Stop callback") {
        objects.Start();
        object.onStop = checkBlocked;
        CHECK_NOTHROW(objects.Stop());
    }
    SECTION("Removal callback during Commit") {
        objects.Start();
        object.onStop = checkBlocked;
        objects.Remove(object);
        CHECK_NOTHROW(objects.Start());
    }
    CHECK_NOTHROW(objects.Stop());
}

TEST_CASE("Engine.Lifecycle.CallbackFailureReleasesTheDispatchGuard", "[engine][lifecycle]") {
    ObjectCalls calls;
    ObjectListProbe object(calls);
    ObjectList objects;
    objects.Add(object);
    const auto fail = [] { throw std::runtime_error("dispatch failure"); };

    SECTION("Start callback") {
        object.onStart = fail;
        CHECK_THROWS_AS(objects.Start(), std::runtime_error);
        object.onStart = {};
    }
    SECTION("Update callback") {
        object.onUpdate = fail;
        CHECK_THROWS_AS(objects.Update(0.1f), std::runtime_error);
        object.onUpdate = {};
    }
    SECTION("Draw callback") {
        objects.Start();
        object.onDraw = fail;
        CHECK_THROWS_AS(PaintFrame([&](Painter& paint) { objects.Draw(paint); }), std::runtime_error);
        object.onDraw = {};
    }
    CHECK_NOTHROW(objects.Update(0.1f));
    CHECK_NOTHROW(PaintFrame([&](Painter& paint) { objects.Draw(paint); }));
    CHECK_NOTHROW(objects.Stop());
    CHECK(calls.starts == 1);
    CHECK(calls.stops == 1);
}
