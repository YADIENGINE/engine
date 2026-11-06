#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>
#include "Graphics/Draw/Elevation.h"
#include "Scene/Scene.h"
#include "Stage/DepthOrder.h"
#include "Stage/Stage.h"
#include "Tests/PaintHarness.h"
#include "Tests/Support/TestSceneServices.h"

namespace {
constexpr float Frame = 1.0f / 60.0f;

// Logs each pass it is asked for, with the view it drew through and whether paint.Camera was open.
class LoggingView final : public StageView {
public:
    void DrawBackground(Painter& paint, const StagePass& pass) const override { Log("background", paint, pass); }
    void DrawFloor(Painter& paint, const StagePass& pass) const override { Log("floor", paint, pass); }
    void DrawWorld(Painter& paint, const StagePass& pass) const override { Log("world", paint, pass); }
    void DrawAir(Painter& paint, const StagePass& pass) const override { Log("air", paint, pass); }
    void DrawGlow(Painter& paint, const StagePass& pass) const override {
        Log("glow", paint, pass);
        paint.Circle(pass.view.target, 30.0f, Fill({40, 220, 255, 255})); // The last thing drawn in a view
    }
    void DrawHud(Painter& paint) const override {
        calls.push_back(paint.CurrentTransform() == Affine2::Identity() ? "hud" : "hud in a camera");
    }
    void DescribeLight(const Painter&, StageLight& light) const override {
        calls.push_back("light");
        light.ambient = {20, 20, 40, 255};
        light.lights.push_back({.at = {640.0f, 360.0f}, .radius = 200.0f});
    }

    mutable std::vector<std::string> calls;
    mutable std::vector<Float2> targets;

private:
    void Log(const char* pass, Painter& paint, const StagePass& stage) const {
        const bool inCamera = paint.CurrentTransform() != Affine2::Identity();
        calls.push_back(std::string(pass) + ":" + std::to_string(stage.camera) + (inCamera ? " world" : ""));
        targets.push_back(stage.view.target);
    }
};

std::vector<std::string> Drawn(const Stage& stage, const LoggingView& view, std::size_t* draws = nullptr) {
    view.calls.clear();
    view.targets.clear();
    PaintHarness harness({1280.0f, 720.0f}, 1.0f);
    harness.Begin();
    stage.Draw(harness.paint, view);
    harness.End();
    CHECK(harness.list.Rejected() == 0);
    if (draws) { // Vertices, since shapes of one kind share a draw
        *draws = 0;
        for (std::size_t i = 0; i < harness.Draws().size(); ++i)
            *draws += harness.Drawn(i).size();
    }
    return view.calls;
}
std::size_t VertexCount(const Stage& stage) {
    std::size_t draws = 0;
    const LoggingView view;
    (void)Drawn(stage, view, &draws);
    return draws;
}
} // namespace

TEST_CASE("Engine.Stage.PassesRunInTheirOrderForEachView", "[engine][stage]") {
    TestSceneServices services;
    Stage stage(services.services);
    stage.Cameras().Split(2);
    stage.UpdateReal(Frame);
    stage.Update(Frame);
    const LoggingView view;
    // World coordinates where the scene draws the world; the background, air and HUD choose their own.
    const std::vector<std::string> split{"background:0", "floor:0 world", "world:0 world", "air:0", "glow:0 world",
                                         "background:1", "floor:1 world", "world:1 world", "air:1", "glow:1 world",
                                         "hud"};
    CHECK(Drawn(stage, view) == split);
}

TEST_CASE("Engine.Stage.OptionalPartsAreOffByDefault", "[engine][stage]") {
    TestSceneServices services;
    Stage stage(services.services);
    CHECK_THROWS_AS(stage.Atmosphere(), std::logic_error);
    CHECK_THROWS_AS(stage.Weather(), std::logic_error);
    CHECK_THROWS_AS(stage.Occluders(), std::logic_error);
    stage.UpdateReal(Frame);
    stage.Update(Frame);
    const LoggingView view;
    PaintHarness harness({1280.0f, 720.0f}, 1.0f);
    harness.Begin();
    stage.Draw(harness.paint, view);
    harness.End();
    CHECK(std::ranges::find(view.calls, "light") == view.calls.end()); // Nobody is asked for lights
    for (const NullCommand* pipeline : harness.device.CommandsOf(NullCommandType::SetPipeline))
        CHECK(pipeline->blend != GpuBlend::Multiply); // Nothing darkens
}

TEST_CASE("Engine.Stage.LightDarkensBeforeTheGlowOnceDescribedForEveryView", "[engine][stage]") {
    TestSceneServices services;
    Stage stage(services.services, {.light = LightLayerOptions{}, .atmosphere = TopDownAtmosphere()});
    stage.Occluders().Line(std::array{Float2{600.0f, 300.0f}, Float2{700.0f, 300.0f}});
    stage.Weather().Snap(Weather::Rain());
    stage.Cameras().Split(2);
    stage.UpdateReal(Frame);
    stage.Update(Frame);
    const LoggingView view;
    PaintHarness harness({1280.0f, 720.0f}, 1.0f);
    harness.Begin();
    stage.Draw(harness.paint, view);
    harness.End();
    CHECK(harness.list.Rejected() == 0);
    CHECK(std::ranges::count(view.calls, "light") == 1); // One frame of light, shown in both views
    CHECK(view.calls.front() == "light");
    // Each view: the world, the light's image stamped over it, then the glow undarkened.
    const auto pipelines = harness.device.CommandsOf(NullCommandType::SetPipeline);
    CHECK(std::ranges::count_if(pipelines, [](const NullCommand* p) { return p->blend == GpuBlend::Multiply; }) == 2);
    CHECK(pipelines.back()->blend != GpuBlend::Multiply);
}

TEST_CASE("Engine.Stage.ViewsGivenFromOutsideReplaceTheOwnCamerasUntilCleared", "[engine][stage]") {
    TestSceneServices services;
    Stage stage(services.services);
    // A camera that lives in a simulation, drawn between its steps: two views of a split screen. Given before the
    // first update, as a constructor does, they are already what the first frame shows.
    const std::array views{
        CameraView{.offset = {320.0f, 360.0f}, .target = {1000.0f, 50.0f}, .viewport = {0.0f, 0.0f, 640.0f, 720.0f}},
        CameraView{.offset = {960.0f, 360.0f}, .target = {-400.0f, 0.0f}, .viewport = {640.0f, 0.0f, 640.0f, 720.0f}}};
    stage.SetViews(views);
    const LoggingView view;
    CHECK(Drawn(stage, view).size() == 11);
    CHECK(view.targets.front() == views[0].target);
    CHECK(view.targets.back() == views[1].target);
    // Heard where they look, and kept through updates until the next SetViews.
    stage.UpdateReal(Frame);
    stage.Update(Frame);
    REQUIRE(services.sounds.Listeners().size() == 2);
    CHECK(services.sounds.Listeners()[1].position == views[1].target);
    CHECK(services.sounds.Listeners()[0].panWidth == Catch::Approx(320.0f));
    (void)Drawn(stage, view);
    CHECK(view.targets.back() == views[1].target);

    stage.SetViews({}); // Back to the stage's own cameras
    stage.UpdateReal(Frame);
    (void)Drawn(stage, view);
    CHECK(view.targets.front() == stage.Cameras().Main().View().target);
    REQUIRE(services.sounds.Listeners().size() == 1);
    CHECK(services.sounds.Listeners()[0].position == stage.Cameras().Main().View().target);
}

TEST_CASE("Engine.Stage.ListenersFollowTheCamerasUnlessTheGamePlacesItsOwn", "[engine][stage]") {
    TestSceneServices services;
    Stage stage(services.services);
    stage.Cameras().Split(3, CameraSplit::Grid);
    stage.Cameras()[2].SetPose({.center = {500.0f, 80.0f}});
    stage.UpdateReal(Frame);
    stage.Update(Frame);
    REQUIRE(services.sounds.Listeners().size() == 3);
    CHECK(services.sounds.Listeners()[2].position == stage.Cameras()[2].View().target);

    TestSceneServices own;
    own.sounds.SetListener({.position = {7.0f, 7.0f}});
    Stage quiet(own.services, {.listeners = false});
    quiet.UpdateReal(Frame);
    quiet.Update(Frame);
    REQUIRE(own.sounds.Listeners().size() == 1);
    CHECK(own.sounds.Listeners()[0].position == Float2{7.0f, 7.0f});
}

TEST_CASE("Engine.Stage.BothClocksReachTheEffects", "[engine][stage]") {
    TestSceneServices services;
    Stage stage(services.services);
    stage.UpdateReal(Frame);
    stage.Update(Frame);
    const std::size_t empty = VertexCount(stage);

    // A mark lies on game time: real time alone does not wear it away, game time does.
    stage.Fx().Mark({640.0f, 360.0f}, {0, 0, 0, 120}, {.seconds = 1.0f, .fadeSeconds = 0.5f});
    stage.UpdateReal(5.0f);
    CHECK(VertexCount(stage) > empty);
    stage.Update(5.0f);
    CHECK(VertexCount(stage) == empty);

    // How long slow motion looks slow is real time: a stopped game does not hold it.
    stage.Fx().SlowMotionLook(1.0f, {255, 255, 255, 255});
    stage.Update(5.0f);
    CHECK(VertexCount(stage) > empty);
    stage.UpdateReal(5.0f);
    CHECK(VertexCount(stage) == empty);
}

TEST_CASE("Engine.Stage.EffectsFollowTheOptionsAndCuesMayChangeTime", "[engine][stage]") {
    TestSceneServices services;
    Stage stage(services.services);
    CHECK(stage.Fx().Quality().flashes);
    services.effects.flashes = false; // The player changed the option
    stage.UpdateReal(Frame);
    stage.Update(Frame);
    CHECK_FALSE(stage.Fx().Quality().flashes);
    stage.Fx().Play(FxCue{FxHitstop{0.1f}}); // The scene's time scale is the stage's
    CHECK(services.time.Advance(Frame) == 0.0f);
}

namespace {
// A scene hosting a stage the usual way: each of its hooks feeds the stage's.
class StagedScene final : public Scene, StageView {
public:
    StagedScene(const SceneServices& services, TimeDomain clock)
        : Scene(clock)
        , stage(services) {}
    Stage stage;

private:
    void UpdateReal(float realSeconds) override { stage.UpdateReal(realSeconds); }
    void Update(float seconds) override {
        stage.Cameras().Main().Follow({5000.0f, 3000.0f});
        stage.Update(seconds);
    }
    void DrawWorld(Painter&, const StagePass&) const override {}
};
// One frame as Game::Run plays it: real time first, then the scene's own clock.
void PlayFrame(StagedScene& scene, TimeScale& time, float real) {
    scene.TickReal(real);
    scene.Tick(scene.Clock() == TimeDomain::Real ? real : time.Advance(real));
}
} // namespace

TEST_CASE("Engine.Stage.AHitstopHoldsTheCamerasButNotWhatIsRead", "[engine][stage]") {
    TestSceneServices services;
    StagedScene scene(services.services, TimeDomain::Game);
    scene.Activate();
    const Float2 before = scene.stage.Cameras().Main().Pose().center;
    const std::size_t empty = VertexCount(scene.stage);
    scene.stage.Fx().Banner("Round 2"); // Real time: reading does not stop for a hitstop
    services.time.Hitstop(5.0f);
    for (int frame = 0; frame < 240; ++frame) // Four real seconds; the game's Update never runs
        PlayFrame(scene, services.time, 1.0f / 60.0f);
    CHECK(scene.stage.Cameras().Main().Pose().center == before); // Held
    CHECK(VertexCount(scene.stage) == empty);                    // The banner has come and gone
    scene.Shutdown();
}

TEST_CASE("Engine.Stage.ARealTimeSceneMovesNothingTwice", "[engine][stage]") {
    TestSceneServices services;
    StagedScene scene(services.services, TimeDomain::Real);
    scene.Activate();
    const std::size_t empty = VertexCount(scene.stage);
    // Both hooks get real seconds; each clock moves only its own parts, so a second of look lasts a second.
    scene.stage.Fx().SlowMotionLook(1.0f, {255, 255, 255, 255});
    scene.stage.Fx().Mark({640.0f, 360.0f}, {0, 0, 0, 120}, {.seconds = 1.0f});
    PlayFrame(scene, services.time, 0.6f);
    CHECK(VertexCount(scene.stage) > empty); // Twice 0.6 would have ended both
    PlayFrame(scene, services.time, 0.6f);
    CHECK(VertexCount(scene.stage) == empty);
    scene.Shutdown();
}

TEST_CASE("Engine.Stage.DepthOrderSortsByGroundAndKeepsLevelThingsInPlace", "[engine][stage]") {
    DepthOrder<int> order;
    for (int frame = 0; frame < 3; ++frame) {
        order.Clear();
        order.Add(200.0f, 1);
        order.Add(100.0f, 2);
        order.Add(200.0f, 3); // Level with 1: after it, every frame
        order.Add(-50.0f, 4);
        order.Add(200.0f, 5);
        const auto sorted = order.Sorted();
        CHECK(std::vector<int>(sorted.begin(), sorted.end()) == std::vector<int>{4, 2, 1, 3, 5});
    }
    struct Thing {
        char kind;
        std::size_t index;
    };
    DepthOrder<Thing> mixed; // Several kinds in one order
    mixed.Add(10.0f, {'t', 0});
    mixed.Add(5.0f, {'e', 3});
    CHECK(mixed.Sorted().front().kind == 'e');
    CHECK_THROWS_AS(mixed.Add(std::nanf(""), {'e', 1}), std::invalid_argument);
}

TEST_CASE("Engine.Stage.RaisedThingsMoveUpAndTheirShadowsFade", "[engine][stage]") {
    PaintHarness harness;
    harness.Begin();
    {
        auto lifted = Raised(harness.paint, 40.0f);
        CHECK(harness.paint.CurrentTransform().Apply({10.0f, 100.0f}).y == Catch::Approx(60.0f));
    }
    CHECK(harness.paint.CurrentTransform() == Affine2::Identity());
    DrawGroundShadow(harness.paint, {100.0f, 100.0f}, 0.0f);
    DrawGroundShadow(harness.paint, {100.0f, 100.0f}, 120.0f, {.fadeHeight = 240.0f});
    DrawGroundShadow(harness.paint, {100.0f, 100.0f}, 240.0f, {.fadeHeight = 240.0f}); // Too high: none
    harness.list.SetStrict(false);
    DrawGroundShadow(harness.paint, {100.0f, 100.0f}, -1.0f);
    harness.End();
    CHECK(harness.list.Rejected() == 1);
    CHECK_FALSE(harness.Draws().empty());
}
