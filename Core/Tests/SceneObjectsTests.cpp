#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <memory>
#include <utility>
#include "Scene/Scene.h"
#include "Tests/PaintHarness.h"

namespace {
struct SceneObjectCalls {
    int starts = 0;
    int updates = 0;
    int draws = 0;
    int stops = 0;
    int destroyed = 0;
};

class SceneObject final : public GameObject {
public:
    explicit SceneObject(SceneObjectCalls& calls)
        : _calls(calls) {}
    ~SceneObject() override { ++_calls.destroyed; }
    std::function<void()> duringUpdate;

private:
    void Start() override { ++_calls.starts; }
    void Update(float seconds) override {
        (void)seconds;
        ++_calls.updates;
        if (duringUpdate) duringUpdate();
    }
    void Draw(Painter&) const override { ++_calls.draws; }
    void Stop() override { ++_calls.stops; }
    SceneObjectCalls& _calls;
};

class ObjectScene final : public Scene {
public:
    ~ObjectScene() override { ShutdownNoThrow(); }
    using Scene::AddObject;
    using Scene::ClearObjects;
    using Scene::RemoveObject;
};
} // namespace

TEST_CASE("Engine.Scene.OwnsDynamicObjectsAndOnlyBorrowsRegisteredReferences", "[engine][scene][lifecycle]") {
    SceneObjectCalls borrowedCalls, ownedCalls;
    SceneObject borrowed(borrowedCalls);
    ObjectScene scene;
    scene.AddObject(borrowed);
    GameObject& owned = scene.AddObject(std::make_unique<SceneObject>(ownedCalls));
    scene.Activate();
    scene.Tick(0.0f);
    PaintFrame([&](Painter& paint) { scene.Render(paint); });
    CHECK(borrowedCalls.starts == 1);
    CHECK(ownedCalls.starts == 1);
    CHECK(ownedCalls.updates == 0);
    CHECK(ownedCalls.draws == 1);

    scene.RemoveObject(owned);
    PaintFrame([&](Painter& paint) { scene.Render(paint); });
    CHECK(ownedCalls.draws == 1);
    CHECK(ownedCalls.destroyed == 0);
    scene.Tick(0.0f);
    CHECK(ownedCalls.stops == 1);
    CHECK(ownedCalls.destroyed == 1);
    scene.ClearObjects();
    scene.Tick(0.0f);
    CHECK(borrowedCalls.stops == 1);
    CHECK(borrowedCalls.destroyed == 0);
    scene.Shutdown();
    CHECK(ownedCalls.stops == 1);
    CHECK(borrowedCalls.stops == 1);
}

TEST_CASE("Engine.Scene.CallbackChangesWaitForDispatchAndZeroTimeCommitsThem", "[engine][scene][lifecycle]") {
    SceneObjectCalls firstCalls, nextCalls;
    ObjectScene scene;
    auto first = std::make_unique<SceneObject>(firstCalls);
    SceneObject* firstObject = first.get();
    first->duringUpdate = [&] {
        scene.RemoveObject(*firstObject);
        scene.AddObject(std::make_unique<SceneObject>(nextCalls));
    };
    scene.AddObject(std::move(first));
    scene.Activate();
    scene.Tick(0.1f);
    PaintFrame([&](Painter& paint) { scene.Render(paint); });
    CHECK(firstCalls.updates == 1);
    CHECK(firstCalls.draws == 0);
    CHECK(firstCalls.destroyed == 0);
    CHECK(nextCalls.starts == 0);
    CHECK(nextCalls.draws == 0);
    scene.Tick(0.0f);
    CHECK(firstCalls.stops == 1);
    CHECK(firstCalls.destroyed == 1);
    CHECK(nextCalls.starts == 1);
    CHECK(nextCalls.updates == 0);
    PaintFrame([&](Painter& paint) { scene.Render(paint); });
    CHECK(nextCalls.draws == 1);
    scene.Shutdown();
    CHECK(nextCalls.stops == 1);
    CHECK(nextCalls.destroyed == 1);
}

TEST_CASE("Engine.Scene.ClearsUnstartedObjectsWithoutStopAndDefersInactiveRemoval", "[engine][scene][lifecycle]") {
    SceneObjectCalls calls;
    ObjectScene scene;
    auto object = std::make_unique<SceneObject>(calls);
    GameObject& registered = scene.AddObject(std::move(object));
    SECTION("A pending object is cleared before activation") {
        scene.ClearObjects();
        scene.Activate();
        CHECK(calls.starts == 0);
        CHECK(calls.stops == 0);
        CHECK(calls.destroyed == 1);
    }
    SECTION("An inactive scene commits removal on reentry") {
        scene.Activate();
        scene.Deactivate();
        scene.RemoveObject(registered);
        scene.Tick(0.0f);
        CHECK(calls.destroyed == 0);
        scene.Activate();
        CHECK(calls.starts == 1);
        CHECK(calls.stops == 1);
        CHECK(calls.destroyed == 1);
    }
    scene.Shutdown();
}
