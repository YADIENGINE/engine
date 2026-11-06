#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "Editor/Api/SceneRegistry.h"
#include "Editor/Internals/Session/WorkerSimulation.h"
#include "Editor/Worker/WorkerScene.h"
#include "Input/Input.h"
#include "Scene/Internals/SceneManager.h"
#include "Simulation/HostedSimulation.h"
#include "Tests/Simulation/ToySimulation.h"
#include "Tests/Support/TestSceneServices.h"

namespace {
using Journal = std::vector<std::string>;
constexpr float Frame = 1.0f / 60.0f;

// One scene class, as a game writes it: services plus its own values, a hosted simulation, editor hooks.
class ToyScene final : public Scene {
public:
    ToyScene(const SceneServices& services, Journal& journal, TimeDomain clock = TimeDomain::Game)
        : Scene(clock)
        , _journal(journal)
        , _hosted(services.simulation, ToyDefinition().settings, SimulationStart{.seed = 1, .players = 2}) {
        _journal.push_back("construct");
    }
    ~ToyScene() override { _journal.push_back("destroy"); }
    [[nodiscard]] std::string ExecuteAction(const std::string& id) override {
        if (id != "bonus") return Scene::ExecuteAction(id);
        _hosted.Runner().Command("bonus", 3);
        return {};
    }
    [[nodiscard]] bool GameTimePaused() const override { return paused; }
    [[nodiscard]] const SimulationRunner& Runner() const { return _hosted.Runner(); }

    bool paused = false;
    bool failEnter = false;
    float realSeconds = 0.0f;
    float updateSeconds = 0.0f;

protected:
    void Start() override { _journal.push_back("start"); }
    void Enter() override {
        _journal.push_back("enter");
        if (failEnter) throw std::runtime_error("enter failed");
    }
    void HandleInput(const DeviceInput&) override { _journal.push_back("input"); }
    void InputCancelled() override {
        _hosted.Runner().CancelInput();
        _journal.push_back("cancel");
    }
    void UpdateReal(float seconds) override { realSeconds += seconds; }
    void Update(float seconds) override {
        updateSeconds += seconds;
        (void)_hosted.Runner().Advance(seconds);
    }
    void Stop() override { _journal.push_back("stop"); }

private:
    Journal& _journal;
    HostedSimulation<ToySimulation> _hosted;
};

SceneDefinition ToyEntry(Journal& journal, ToyScene*& made, TimeDomain clock = TimeDomain::Game) {
    SceneDefinition definition;
    definition.id = "test.toy";
    definition.name = "Toy";
    definition.category = "Test";
    definition.actions = {{"bonus", "Bonus", "A command on the tape.", true}};
    definition.create = [&journal, &made, clock](const SceneServices& services, const SceneParameters&) {
        auto scene = std::make_unique<ToyScene>(services, journal, clock);
        made = scene.get();
        return scene;
    };
    return definition;
}
} // namespace

TEST_CASE("Editor.OneScene.TheGameHostsItWithoutASimulationHost", "[engine-editor][scene]") {
    TestSceneServices game;
    Journal journal;
    ToyScene scene(game.services, journal);
    SceneManager scenes;
    scenes.Register(scene);
    scenes.Request(scene);
    REQUIRE(scenes.Commit({}));
    CHECK(journal == Journal{"construct", "start", "enter"});
    DeviceInput input;
    for (int frame = 0; frame < 30; ++frame) {
        scenes.Current()->ReadInput(input);
        scenes.Current()->Tick(game.time.Advance(Frame));
    }
    CHECK(scene.Runner().StepIndex() == 30); // A null host: the simulation simply runs
    CHECK(scene.ExecuteAction("bonus").empty());
    CHECK(scene.ExecuteAction("other") == "Unsupported action: other");
    scenes.Shutdown();
    CHECK(scene.IsShutdown());
    CHECK(journal.back() == "stop");
}

TEST_CASE("Editor.OneScene.TheWorkerHostsTheSameClass", "[engine-editor][scene]") {
    WorkerSimulation worker;
    TestSceneServices editor(&worker);
    Journal journal;
    ToyScene* made = nullptr;
    const auto definition = ToyEntry(journal, made);
    {
        WorkerScene host(editor.services);
        host.Activate();
        host.Open(definition, {});
        CHECK(journal == Journal{"construct", "start", "enter"}); // Started before input or drawing
        REQUIRE(worker.Runner() == &made->Runner());              // The editor's host keeps the tape

        // Game::Run's order: real time first, then GameTimePaused, then game time.
        host.TickReal(Frame);
        CHECK_FALSE(host.GameTimePaused());
        host.Tick(Frame);
        CHECK(made->realSeconds == Catch::Approx(Frame));
        CHECK(made->updateSeconds == Catch::Approx(Frame));
        CHECK(worker.Runner()->StepIndex() == 1);
        CHECK(host.Seconds() == Catch::Approx(Frame)); // The draw clock follows the editor's real time
        made->paused = true;
        CHECK(host.GameTimePaused());

        host.ReadInput(DeviceInput{});
        CHECK(journal.back() == "input");
        host.CancelInput();
        CHECK(journal.back() == "cancel");
        CHECK(host.Action("bonus").empty());
        CHECK(host.Action("missing") == "Unknown scene action: missing");

        journal.clear();
        host.Reset(); // The old instance is gone before the factory makes the next one
        CHECK(journal == Journal{"cancel", "stop", "destroy", "construct", "start", "enter"});
        CHECK(host.Seconds() == 0.0);
        REQUIRE(worker.Runner() == &made->Runner());

        journal.clear();
        host.Shutdown();
        CHECK(journal == Journal{"cancel", "stop", "destroy"}); // Once: the hosted scene's shutdown cancels it
        CHECK_FALSE(host.HasContent());
        CHECK(worker.Runner() == nullptr);
    }
}

TEST_CASE("Editor.OneScene.TheWorkerKeepsTheScenesClock", "[engine-editor][scene]") {
    WorkerSimulation worker;
    TestSceneServices editor(&worker);
    Journal journal;
    ToyScene* made = nullptr;
    const auto definition = ToyEntry(journal, made, TimeDomain::Real);
    WorkerScene host(editor.services);
    host.Activate();
    host.Open(definition, {});
    host.TickReal(0.1f); // A real-time scene updates with real time, whatever game time does
    host.Tick(0.05f);
    CHECK(made->realSeconds == Catch::Approx(0.1f));
    CHECK(made->updateSeconds == Catch::Approx(0.1f));
    host.Shutdown();
}

TEST_CASE("Editor.OneScene.TheWorkerHoldsContentOnlyWhileItRuns", "[engine-editor][scene]") {
    WorkerSimulation worker;
    TestSceneServices editor(&worker);
    Journal journal;
    ToyScene* made = nullptr;
    const auto definition = ToyEntry(journal, made);
    {
        WorkerScene host(editor.services); // Never activated: no content, so nothing to shut down
        CHECK_THROWS_AS(host.Open(definition, {}), std::logic_error);
        CHECK_FALSE(host.HasContent());
    }
    CHECK(journal.empty());
    WorkerScene host(editor.services);
    host.Activate();
    host.Open(definition, {});
    host.Deactivate(); // Content stays until the worker stops
    CHECK(host.HasContent());
    journal.clear();
    host.Shutdown();
    CHECK(journal == Journal{"cancel", "stop", "destroy"});
    CHECK_THROWS_AS(host.Reset(), std::logic_error);
    CHECK(worker.Runner() == nullptr);
}

TEST_CASE("Editor.OneScene.AFailedActivationIsShutDownBeforeTheInstanceDies", "[engine-editor][scene]") {
    WorkerSimulation worker;
    TestSceneServices editor(&worker);
    Journal journal;
    ToyScene* made = nullptr;
    auto definition = ToyEntry(journal, made);
    definition.create = [&journal](const SceneServices& services, const SceneParameters&) {
        auto scene = std::make_unique<ToyScene>(services, journal);
        scene->failEnter = true;
        return scene;
    };
    WorkerScene host(editor.services);
    host.Activate();
    CHECK_THROWS_WITH(host.Open(definition, {}), "enter failed");
    CHECK(journal == Journal{"construct", "start", "enter", "cancel", "stop", "destroy"});
    CHECK_FALSE(host.HasContent());
    host.Shutdown();
}
