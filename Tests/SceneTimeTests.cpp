#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
#include "Scene/Scene.h"

namespace {
class TimeSceneChild final : public GameObject {
public:
    explicit TimeSceneChild(std::vector<std::string>& log)
        : _log(log) {}

private:
    void Update(float seconds) override {
        (void)seconds;
        _log.push_back("child.update");
    }
    std::vector<std::string>& _log;
};

class TimeScene final : public Scene {
public:
    explicit TimeScene(std::vector<std::string>& log)
        : Scene(TimeDomain::Game)
        , _log(log)
        , _child(log) {
        AddObject(_child);
    }
    ~TimeScene() override { ShutdownNoThrow(); }
    [[nodiscard]] bool GameTimePaused() const override { return paused; }
    bool paused = true;
    float realElapsed = 0.0f;
    float gameElapsed = 0.0f;
    std::function<void()> duringReal;

private:
    void Start() override { _log.push_back("scene.start"); }
    void Enter() override { _log.push_back("scene.enter"); }
    void UpdateReal(float seconds) override {
        _log.push_back("scene.real");
        realElapsed += seconds;
        if (duringReal) duringReal();
    }
    void Update(float seconds) override {
        _log.push_back("scene.update");
        gameElapsed += seconds;
    }
    void Draw() const override { _log.push_back("scene.draw"); }
    std::vector<std::string>& _log;
    TimeSceneChild _child;
};
} // namespace

TEST_CASE("Engine.Scene.RealUpdateRunsWhileGameTimeIsFrozenAndCanReleasePause", "[engine][scene][time]") {
    std::vector<std::string> log;
    TimeScene scene(log);
    scene.Activate();
    log.clear();
    scene.TickReal(0.1f);
    CHECK(scene.GameTimePaused());
    scene.Tick(0.0f);
    scene.Render();
    CHECK(log == std::vector<std::string>{"scene.real", "scene.draw"});
    CHECK(scene.realElapsed == Catch::Approx(0.1f));
    CHECK(scene.gameElapsed == 0.0f);

    log.clear();
    scene.duringReal = [&] { scene.paused = false; };
    scene.TickReal(0.1f);
    CHECK_FALSE(scene.GameTimePaused());
    scene.Tick(0.1f);
    CHECK(log == std::vector<std::string>{"scene.real", "child.update", "scene.update"});
    CHECK(scene.gameElapsed == Catch::Approx(0.1f));
}

TEST_CASE("Engine.Scene.ZeroHostTimeInitializesWithoutAdvancingEitherUpdate", "[engine][scene][time]") {
    std::vector<std::string> log;
    TimeScene scene(log);
    scene.Activate();
    scene.TickReal(0.0f);
    scene.Tick(0.0f);
    scene.Render();
    CHECK(log == std::vector<std::string>{"scene.start", "scene.enter", "scene.draw"});
    CHECK(scene.realElapsed == 0.0f);
    CHECK(scene.gameElapsed == 0.0f);
    log.clear();
    scene.TickReal(0.2f);
    scene.Tick(0.05f);
    CHECK(scene.realElapsed == Catch::Approx(0.2f));
    CHECK(scene.gameElapsed == Catch::Approx(0.05f));
}

TEST_CASE("Engine.Scene.RealUpdateRespectsActivationAndReleasesItsDispatchGuardOnFailure", "[engine][scene][time]") {
    std::vector<std::string> log;
    TimeScene scene(log);
    scene.SetEnabled(false);
    scene.Activate();
    scene.TickReal(0.1f);
    CHECK(log.empty());
    scene.SetEnabled(true);
    scene.duringReal = [&] {
        CHECK_THROWS_AS(scene.TickReal(0.1f), std::logic_error);
        CHECK_THROWS_AS(scene.Tick(0.1f), std::logic_error);
        CHECK_THROWS_AS(scene.Render(), std::logic_error);
        CHECK_THROWS_AS(scene.Deactivate(), std::logic_error);
        CHECK_THROWS_AS(scene.Shutdown(), std::logic_error);
        throw std::runtime_error("real update failure");
    };
    CHECK_THROWS_WITH(scene.TickReal(0.1f), "real update failure");
    scene.duringReal = {};
    CHECK_NOTHROW(scene.TickReal(0.1f));
    log.clear();
    scene.Deactivate();
    scene.TickReal(0.1f);
    CHECK(log.empty());
}
