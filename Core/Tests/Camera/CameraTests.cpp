#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "Camera/Camera2D.h"
#include "Camera/Cameras.h"
#include "Camera/CameraView.h"
#include "Camera/Internals/CameraSolver.h"

using Catch::Approx;

namespace {
constexpr FloatRect Canvas{0.0f, 0.0f, 800.0f, 400.0f};
constexpr float Frame = 1.0f / 60.0f;

CameraSettings Instant() { return {.follow = {.smoothTime = 0.0f}}; }
void Run(Camera2D& camera, Float2 target, int frames, float seconds = Frame) {
    for (int i = 0; i < frames; ++i) {
        camera.Follow(target);
        camera.Update(seconds);
    }
}
// Every field State writes, in order, as text: two cameras with the same inputs must write the same.
struct Recorder {
    std::vector<std::string> fields;
    void Field(std::string_view name, float value) { fields.push_back(std::string(name) + std::to_string(value)); }
    void Field(std::string_view name, bool value) { fields.push_back(std::string(name) + (value ? "1" : "0")); }
    void Field(std::string_view name, Float2 value) {
        fields.push_back(std::string(name) + std::to_string(value.x) + "," + std::to_string(value.y));
    }
    template <class Body> void Group(std::string_view name, std::size_t index, Body&& body) {
        fields.push_back(std::string(name) + std::to_string(index));
        body();
    }
};
} // namespace

TEST_CASE("Engine.Camera.StartsShowingTheCanvasAsIs", "[engine][camera]") {
    const Camera2D camera(Canvas);
    CHECK(camera.Pose().center == Canvas.Center());
    const CameraView view = camera.View();
    CHECK(view.ToScreen({123.0f, 45.0f}).x == Approx(123.0f));
    CHECK(view.ToScreen({123.0f, 45.0f}).y == Approx(45.0f));
    CHECK(camera.VisibleArea() == Canvas);
    CHECK(view.viewport == Canvas);
}
TEST_CASE("Engine.Camera.FollowsATarget", "[engine][camera]") {
    Camera2D camera(Canvas, Instant());
    Run(camera, {1000.0f, -50.0f}, 1);
    CHECK(camera.Pose().center == Float2{1000.0f, -50.0f});
    CHECK(camera.ToCanvas({1000.0f, -50.0f}).x == Approx(400.0f));
    CHECK(camera.ToWorld({400.0f, 200.0f}).x == Approx(1000.0f));
    CHECK(camera.Shows({1390.0f, -50.0f}));
    CHECK_FALSE(camera.Shows({1410.0f, -50.0f}));
    CHECK(camera.Shows({1410.0f, -50.0f}, 20.0f));
}
TEST_CASE("Engine.Camera.DeadZoneLetsTheTargetMoveFreely", "[engine][camera]") {
    // A dead zone a quarter of the view wide: 200 x 100 world units at zoom 1.
    Camera2D camera(Canvas, {.follow = {.deadZone = {0.25f, 0.25f}, .smoothTime = 0.0f}});
    const Float2 start = camera.Pose().center;
    Run(camera, start + Float2{90.0f, -40.0f}, 1);
    CHECK(camera.Pose().center == start); // Inside: nothing moves
    Run(camera, start + Float2{150.0f, 0.0f}, 1);
    CHECK(camera.Pose().center.x == Approx(start.x + 50.0f)); // Pushed at the zone's edge
    CHECK(camera.Pose().center.y == Approx(start.y));
    Run(camera, start + Float2{110.0f, 0.0f}, 1);
    CHECK(camera.Pose().center.x == Approx(start.x + 50.0f)); // Back inside: stays
}
TEST_CASE("Engine.Camera.SmoothingArrivesWithoutOvershootAtAnyFrameRate", "[engine][camera]") {
    const auto follow = [](float seconds, int frames) {
        Camera2D camera(Canvas, {.follow = {.smoothTime = 0.2f}});
        const Float2 target{1400.0f, 200.0f};
        float previous = camera.Pose().center.x;
        for (int i = 0; i < frames; ++i) {
            Run(camera, target, 1, seconds);
            CHECK(camera.Pose().center.x >= previous);
            CHECK(camera.Pose().center.x <= target.x);
            previous = camera.Pose().center.x;
        }
        return camera.Pose().center.x;
    };
    const float at60 = follow(1.0f / 60.0f, 30);
    const float at30 = follow(1.0f / 30.0f, 15);
    const float at144 = follow(1.0f / 144.0f, 72);
    CHECK(at60 > 1000.0f);
    CHECK(at60 < 1400.0f);
    CHECK(at30 == Approx(at60).margin(15.0f)); // Half a second either way, near the same place
    CHECK(at144 == Approx(at60).margin(15.0f));
    CHECK(follow(Frame, 600) == Approx(1400.0f));
}
TEST_CASE("Engine.Camera.LooksAheadOfMotionWithinItsLimit", "[engine][camera]") {
    Camera2D camera(Canvas, {.follow = {.smoothTime = 0.0f, .lookAhead = {0.5f, 0.0f}, .lookAheadLimit = 0.1f,
                                        .lookAheadSmoothTime = 0.0f}});
    Float2 target{400.0f, 200.0f};
    for (int i = 0; i < 30; ++i) { // 120 units a second to the right
        target.x += 2.0f;
        Run(camera, target, 1);
    }
    CHECK(camera.Focus().x == Approx(target.x + 60.0f)); // Half a second ahead
    CHECK(camera.Focus().y == Approx(target.y));         // Not on this axis
    for (int i = 0; i < 30; ++i) { // 1200 units a second: limited to a tenth of the width
        target.x += 20.0f;
        Run(camera, target, 1);
    }
    CHECK(camera.Focus().x == Approx(target.x + 80.0f));
    Run(camera, target, 1); // Stopped
    CHECK(camera.Focus().x == Approx(target.x));
}
TEST_CASE("Engine.Camera.StaysInsideItsLimits", "[engine][camera]") {
    const FloatRect level{0.0f, 0.0f, 2000.0f, 300.0f}; // Lower than the view
    Camera2D camera(Canvas, {.follow = {.smoothTime = 0.0f}, .limits = level});
    CHECK(camera.VisibleArea().x == Approx(0.0f));
    CHECK(camera.Pose().center.y == Approx(150.0f)); // Centred on the axis the view is larger on
    Run(camera, {-500.0f, 900.0f}, 1);
    CHECK(camera.VisibleArea().x == Approx(0.0f));
    CHECK(camera.Pose().center.y == Approx(150.0f));
    Run(camera, {1900.0f, 0.0f}, 1);
    CHECK(camera.VisibleArea().Right() == Approx(2000.0f));
    camera.SetPose({.center = {5000.0f, 0.0f}});
    CHECK(camera.VisibleArea().Right() == Approx(2000.0f));
    camera.SetLimits(std::nullopt);
    camera.SetPose({.center = {5000.0f, 0.0f}});
    CHECK(camera.Pose().center.x == 5000.0f);
}
TEST_CASE("Engine.Camera.ZoomsToFitSeveralTargets", "[engine][camera]") {
    Camera2D camera(Canvas, {.follow = {.smoothTime = 0.0f},
                             .zoom = {.fit = true, .min = 0.25f, .max = 1.0f, .padding = 50.0f, .smoothTime = 0.0f}});
    const auto both = [&](Float2 a, Float2 b) {
        camera.Follow({{.position = a, .radius = 20.0f}, {.position = b, .radius = 20.0f}});
        camera.Update(Frame);
    };
    both({0.0f, 0.0f}, {100.0f, 0.0f});
    CHECK(camera.Pose().zoom == Approx(1.0f)); // Close together: the most zoom allowed
    both({0.0f, 0.0f}, {1360.0f, 0.0f});        // 1400 wide with radii, 700 to fit in: half
    CHECK(camera.Pose().zoom == Approx(0.5f));
    CHECK(camera.Shows({-20.0f, 0.0f}));
    CHECK(camera.Shows({1380.0f, 0.0f}));
    both({0.0f, 0.0f}, {10000.0f, 0.0f}); // Too far: held at min, centred between them
    CHECK(camera.Pose().zoom == Approx(0.25f));
    CHECK(camera.Pose().center.x == Approx(5000.0f));
}
TEST_CASE("Engine.Camera.WeightsPullTheCameraButFittingKeepsEveryone", "[engine][camera]") {
    Camera2D camera(Canvas, Instant());
    camera.Follow({{.position = {0.0f, 0.0f}, .weight = 3.0f}, {.position = {400.0f, 0.0f}, .weight = 1.0f},
                   {.position = {9000.0f, 9000.0f}, .weight = 0.0f}});
    camera.Update(Frame);
    CHECK(camera.Pose().center.x == Approx(100.0f));
    CHECK(camera.Pose().center.y == Approx(0.0f));

    Camera2D fitting(Canvas, {.follow = {.smoothTime = 0.0f}, .zoom = {.fit = true, .min = 1.0f, .max = 1.0f}});
    fitting.Follow({{.position = {0.0f, 0.0f}, .weight = 9.0f}, {.position = {700.0f, 0.0f}, .weight = 1.0f}});
    fitting.Update(Frame);
    CHECK(fitting.Shows({700.0f, 0.0f})); // The weighted point alone would leave it out
    CHECK(fitting.Shows({0.0f, 0.0f}));
}
TEST_CASE("Engine.Camera.SnapArrivesAndForgetsMotion", "[engine][camera]") {
    Camera2D camera(Canvas, {.follow = {.deadZone = {0.5f, 0.5f}, .smoothTime = 1.0f, .lookAhead = {1.0f, 1.0f}}});
    Run(camera, {300.0f, 100.0f}, 10);
    camera.Follow({5000.0f, 5000.0f});
    camera.Snap();
    CHECK(camera.Pose().center == Float2{5000.0f, 5000.0f});
    CHECK(camera.View(0.0f).target == camera.View(1.0f).target); // Nothing to draw between
    Run(camera, {5000.0f, 5000.0f}, 1);
    CHECK(camera.Pose().center == Float2{5000.0f, 5000.0f}); // No speed left over
}
TEST_CASE("Engine.Camera.ViewBetweenUpdatesForASimulation", "[engine][camera]") {
    Camera2D camera(Canvas, Instant());
    Run(camera, {100.0f, 0.0f}, 1);
    Run(camera, {200.0f, 0.0f}, 1);
    CHECK(camera.View(0.0f).target.x == Approx(100.0f));
    CHECK(camera.View(0.5f).target.x == Approx(150.0f));
    CHECK(camera.View(1.0f).target.x == Approx(200.0f));
    CHECK(camera.View().target.x == Approx(200.0f));
    camera.Update(0.0f); // A step of no time: the pose holds, and so does the view between
    CHECK(camera.View(0.0f).target.x == Approx(200.0f));
}
TEST_CASE("Engine.Camera.SameInputsGiveTheSameState", "[engine][camera]") {
    const auto run = [] {
        Camera2D camera(Canvas, {.follow = {.deadZone = {0.2f, 0.1f}, .smoothTime = 0.3f, .lookAhead = {0.4f, 0.2f}},
                                 .zoom = {.fit = true, .min = 0.4f, .padding = 30.0f},
                                 .limits = FloatRect{-3000.0f, -3000.0f, 6000.0f, 6000.0f}});
        for (int step = 0; step < 600; ++step) {
            const float t = static_cast<float>(step) * Frame;
            camera.Follow({{.position = {std::sin(t) * 900.0f, t * 40.0f}, .radius = 16.0f},
                           {.position = {std::cos(t * 1.3f) * 700.0f, -t * 30.0f}, .weight = 2.0f}});
            camera.Update(Frame);
        }
        Recorder recorder;
        camera.State(recorder);
        return recorder.fields;
    };
    const auto first = run();
    CHECK(first.size() > 10);
    CHECK(first == run());
}
TEST_CASE("Engine.Camera.StopFollowingStaysPut", "[engine][camera]") {
    Camera2D camera(Canvas, Instant());
    Run(camera, {900.0f, 900.0f}, 1);
    camera.StopFollowing();
    camera.Update(Frame);
    CHECK(camera.Pose().center == Float2{900.0f, 900.0f});
    CHECK(camera.Targets().empty());
}
TEST_CASE("Engine.Camera.PixelSnapMovesTheWorldByWholeUnits", "[engine][camera]") {
    Camera2D camera(Canvas, {.follow = {.smoothTime = 0.0f}, .zoom = {.zoom = 1.5f}, .pixelSnap = true});
    Run(camera, {100.37f, 52.81f}, 1);
    const CameraView view = camera.View();
    const Float2 shift = view.offset - view.target * view.zoom;
    CHECK(shift.x == Approx(std::round(shift.x)).margin(1e-3f));
    CHECK(shift.y == Approx(std::round(shift.y)).margin(1e-3f));
    CHECK(std::fabs(view.target.x - 100.37f) <= 1.0f / 1.5f);
}
TEST_CASE("Engine.Camera.RefusesInvalidSettings", "[engine][camera]") {
    CHECK_THROWS_AS(Camera2D(Canvas, {.follow = {.deadZone = {1.5f, 0.0f}}}), std::invalid_argument);
    CHECK_THROWS_AS(Camera2D(Canvas, {.zoom = {.zoom = 0.0f}}), std::invalid_argument);
    CHECK_THROWS_AS(Camera2D(Canvas, {.zoom = {.min = 2.0f, .max = 1.0f}}), std::invalid_argument);
    CHECK_THROWS_AS(Camera2D(Canvas, {.limits = FloatRect{}}), std::invalid_argument);
    CHECK_THROWS_AS(Camera2D(Canvas, {.follow = {.smoothTime = NAN}}), std::invalid_argument);
    Camera2D camera(Canvas);
    CHECK_THROWS_AS(camera.SetSettings({.follow = {.lookAheadLimit = -1.0f}}), std::invalid_argument);
}

TEST_CASE("Engine.Camera.ViewAreaAndConfining", "[engine][camera]") {
    const CameraView view{.offset = {100.0f, 50.0f}, .target = {0.0f, 0.0f}, .zoom = 2.0f,
                          .viewport = {0.0f, 0.0f, 200.0f, 100.0f}};
    CHECK(view.VisibleArea() == FloatRect{-50.0f, -25.0f, 100.0f, 50.0f});
    const CameraView confined = view.ConfinedTo({0.0f, 0.0f, 1000.0f, 30.0f});
    CHECK(confined.VisibleArea().x == Approx(0.0f));
    CHECK(confined.target.y == Approx(15.0f)); // 50 tall in 30: centred
    CHECK(CameraView{}.VisibleArea().Empty());
    CHECK(CameraView{}.ConfinedTo({0.0f, 0.0f, 10.0f, 10.0f}).target == Float2{});
}
TEST_CASE("Engine.Camera.SolverRules", "[engine][camera]") {
    CHECK(ApplyDeadZone({0.0f, 0.0f}, {15.0f, -3.0f}, {10.0f, 5.0f}) == Float2{5.0f, 0.0f});
    const CameraGroup group = GatherTargets(std::vector<CameraTarget>{
        {.position = {0.0f, 0.0f}, .radius = 10.0f}, {.position = {100.0f, 50.0f}, .weight = 0.0f}});
    CHECK(group.any);
    CHECK(group.bounds == FloatRect{-10.0f, -10.0f, 20.0f, 20.0f});
    CHECK_FALSE(GatherTargets(std::vector<CameraTarget>{{.weight = 0.0f}}).any);
    CHECK(FitZoom({0.0f, 0.0f, 0.0f, 0.0f}, Canvas, {.min = 0.5f, .max = 2.0f}) == 2.0f);
    CHECK(FitZoom({0.0f, 0.0f, 1600.0f, 10.0f}, Canvas, {.min = 0.1f, .max = 2.0f}) == Approx(0.5f));
    CHECK(KeepInView({0.0f, 0.0f}, {90.0f, -10.0f, 20.0f, 20.0f}, {200.0f, 200.0f}, 0.0f).x == Approx(10.0f));
    CHECK(KeepInView({0.0f, 0.0f}, {0.0f, 0.0f, 500.0f, 10.0f}, {200.0f, 200.0f}, 0.0f).x == Approx(250.0f));
    CHECK(Confine({0.0f, 0.0f}, {100.0f, 100.0f}, {0.0f, 0.0f, 1000.0f, 50.0f}) == Float2{50.0f, 25.0f});
}

TEST_CASE("Engine.Cameras.MainAndSplitScreen", "[engine][camera]") {
    Cameras cameras(Canvas, Instant());
    CHECK(cameras.Count() == 1);
    cameras.Main().Follow({1000.0f, 1000.0f});
    cameras.Update(Frame);
    cameras.Split(2, CameraSplit::SideBySide, 10.0f);
    REQUIRE(cameras.Count() == 2);
    CHECK(cameras[0].Viewport() == FloatRect{0.0f, 0.0f, 395.0f, 400.0f});
    CHECK(cameras[1].Viewport() == FloatRect{405.0f, 0.0f, 395.0f, 400.0f});
    CHECK(cameras[1].Pose().center == cameras.Main().Pose().center); // A copy of the main camera
    CHECK(cameras.At({100.0f, 100.0f}) == &cameras[0]);
    CHECK(cameras.At({400.0f, 100.0f}) == nullptr); // In the gap
    CHECK(cameras.At({500.0f, 100.0f}) == &cameras[1]);

    cameras[1].Follow({-200.0f, 0.0f});
    cameras.Update(Frame);
    CHECK(cameras[1].Pose().center == Float2{-200.0f, 0.0f});
    CHECK(cameras.Main().Pose().center == Float2{1000.0f, 1000.0f});

    cameras.Split(3, CameraSplit::Grid);
    CHECK(cameras[2].Viewport() == FloatRect{0.0f, 200.0f, 800.0f, 200.0f});
    cameras.Split(2, CameraSplit::Stacked);
    CHECK(cameras[1].Viewport() == FloatRect{0.0f, 200.0f, 800.0f, 200.0f});
    cameras.SetCanvas({0.0f, 0.0f, 1600.0f, 800.0f});
    CHECK(cameras[1].Viewport() == FloatRect{0.0f, 400.0f, 1600.0f, 400.0f});
    cameras.Split(1);
    CHECK(cameras.Main().Viewport() == FloatRect{0.0f, 0.0f, 1600.0f, 800.0f});
    CHECK_THROWS_AS(cameras.Split(5), std::invalid_argument);
    CHECK_THROWS_AS(cameras.Split(0), std::invalid_argument);
    CHECK_THROWS_AS(cameras[3], std::out_of_range);
    std::size_t count = 0;
    for (const Camera2D& camera : cameras)
        count += camera.Viewport().Empty() ? 0U : 1U;
    CHECK(count == 1);
}
