#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include "Camera/Cameras.h"
#include "Camera/Debug/CameraGuides.h"
#include "Fx/FxStage.h"
#include "Graphics/Style/Theme.h"
#include "Tests/PaintHarness.h"

using Catch::Approx;

namespace {
constexpr FloatRect Canvas{0.0f, 0.0f, 400.0f, 200.0f};
} // namespace

TEST_CASE("Engine.Camera.PaintThroughAViewClipsAndTransforms", "[engine][camera]") {
    Cameras cameras(Canvas, {.follow = {.smoothTime = 0.0f}});
    cameras.Split(2, CameraSplit::SideBySide);
    cameras[1].Follow({1000.0f, 500.0f});
    cameras.Update(1.0f / 60.0f);
    PaintHarness h;
    h.Begin();
    {
        auto world = h.paint.Camera(cameras[1].View());
        const FloatRect clip = h.list.State().clip.value_or(FloatRect{});
        CHECK(clip == FloatRect{200.0f, 0.0f, 200.0f, 200.0f});
        const Float2 middle = h.list.State().transform.Apply({1000.0f, 500.0f});
        CHECK(middle.x == Approx(300.0f)); // The middle of the right half
        CHECK(middle.y == Approx(100.0f));
        h.paint.Circle({1000.0f, 500.0f}, 10.0f, Fill(Color4{255, 255, 255, 255}));
    }
    CHECK_FALSE(h.list.State().clip.has_value()); // One scope undoes both
    CHECK(h.list.State().transform == Affine2::Identity());
    {
        auto unclipped = h.paint.Camera(CameraView{.offset = {10.0f, 0.0f}});
        CHECK_FALSE(h.list.State().clip.has_value());
        CHECK(h.list.State().transform.Apply({0.0f, 0.0f}).x == Approx(10.0f));
    }
    h.End();
    CHECK(h.Draws().size() == 1);
}

TEST_CASE("Engine.Camera.GuidesDrawEveryPart", "[engine][camera]") {
    Camera2D camera(Canvas, {.follow = {.deadZone = {0.3f, 0.3f}, .lookAhead = {0.3f, 0.0f}},
                             .limits = FloatRect{-500.0f, -500.0f, 2000.0f, 1000.0f}});
    camera.Follow({{.position = {100.0f, 100.0f}, .radius = 12.0f}, {.position = {300.0f, 120.0f}}});
    camera.Update(1.0f / 60.0f);
    PaintHarness h;
    h.Begin();
    DrawCameraGuides(h.paint, camera);
    h.End();
    REQUIRE_FALSE(h.Draws().empty());
    CHECK_FALSE(h.Drawn().empty());
    CHECK(h.list.Rejected() == 0);
}

TEST_CASE("Engine.Camera.EffectsBlendOverTheGameCamera", "[engine][camera]") {
    FxStage stage(Canvas, Canvas, CurrentTheme().fx);
    Cameras cameras(Canvas, {.follow = {.smoothTime = 0.0f}, .limits = FloatRect{0.0f, 0.0f, 1000.0f, 200.0f}});
    cameras.Split(2, CameraSplit::SideBySide);
    cameras[1].Follow({500.0f, 100.0f});
    cameras.Update(1.0f / 60.0f);
    const auto& camera = cameras[1];
    PaintHarness h;
    h.Begin();
    const CameraView still = stage.WorldCamera(h.paint, camera.View(), camera.Settings().limits);
    CHECK(still.target == camera.View().target);
    CHECK(still.viewport == camera.Viewport()); // Effects keep the view's place on the canvas

    // A shot towards the far edge: held inside the limits only when it asks to be.
    stage.SetShot({.focus = {990.0f, 100.0f}, .zoom = 1.0f, .weight = 1.0f, .keepInArena = true});
    const CameraView kept = stage.WorldCamera(h.paint, camera.View(), camera.Settings().limits);
    CHECK(kept.VisibleArea().Right() == Approx(1000.0f));
    CHECK(kept.offset == camera.View().offset);
    stage.SetShot({.focus = {990.0f, 100.0f}, .zoom = 1.0f, .weight = 1.0f, .keepInArena = false});
    CHECK(stage.WorldCamera(h.paint, camera.View(), camera.Settings().limits).target.x == Approx(990.0f));
    h.End();
}
