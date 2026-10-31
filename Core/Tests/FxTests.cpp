#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Fx/CameraEffects2D.h"
#include "Fx/CameraShot.h"
#include "Fx/FxQuality.h"
#include "Fx/FxStage.h"
#include "Fx/IrisTransition.h"
#include "Fx/ParticleSystem2D.h"
#include "Graphics/Text/FontFace.h"
#include "Tests/PaintHarness.h"
#include "Utilities/MathUtils.h"

using Catch::Approx;

namespace {
constexpr Color4 Pink{255, 60, 180, 255};
constexpr FloatRect Area{0.0f, 0.0f, 400.0f, 200.0f};

const FontFace& TestFont() {
    static const FontFace font = FontFace::FromFile(ENGINE_TEST_FONT);
    return font;
}

constexpr ParticleEmission Emission{
    .speed = {50.0f, 100.0f}, .lifetime = {0.5f, 1.0f}, .size = {2.0f, 3.0f}, .acceleration = {0.0f, 100.0f}};

constexpr CameraEffectsSettings Camera{.shakeAmplitude = 8.0f,
                                       .shakeFrequency = {80.0f, 60.0f},
                                       .shakeFadeSeconds = 0.2f,
                                       .punchDecay = 8.0f,
                                       .kickImpulse = 100.0f,
                                       .kickStiffness = 400.0f,
                                       .kickDamping = 20.0f};

FxStyle TestFx() {
    FxStyle style;
    style.particleCapacity = 50;
    style.toastCapacity = 2;
    style.particles.emission = Emission;
    style.camera = Camera;
    style.ring = {.duration = 0.5f, .endRadius = 40.0f, .thickness = 3.0f};
    style.flash = {.duration = 0.3f, .opacity = 0.5f};
    style.shockwave = {.duration = 0.5f, .width = 10.0f};
    style.toast = {.duration = 1.0f, .text = {.size = 20.0f}};
    style.text = {.duration = 1.0f, .text = {.size = 18.0f}};
    return style;
}
} // namespace

TEST_CASE("Engine.Fx.ParticlesRespectCapacityQualityAndLifetime", "[engine][fx]") {
    ParticleSystem2D particles({.capacity = 10, .seed = 7U});
    particles.Emit({10.0f, 10.0f}, 25, Emission, {.color = Pink});
    CHECK(particles.ActiveCount() == 10);
    particles.Update(1.1f); // Longer than the longest lifetime
    CHECK(particles.ActiveCount() == 0);

    particles.SetQuality({.particlePercent = 50});
    particles.Emit({10.0f, 10.0f}, 8, Emission, {.color = Pink});
    CHECK(particles.ActiveCount() == 4);
    particles.Clear();
    particles.SetQuality({.particlePercent = 0});
    particles.Emit({10.0f, 10.0f}, 8, Emission, {.color = Pink});
    CHECK(particles.ActiveCount() == 0);

    CHECK_THROWS_AS(particles.Emit({}, 1, {.lifetime = {0.0f, 1.0f}}, {}), std::invalid_argument);
}

TEST_CASE("Engine.Fx.ParticlesOfBothBlendsDrawInOneBatch", "[engine][fx]") {
    ParticleSystem2D particles({.capacity = 20, .seed = 3U});
    particles.Emit({50.0f, 50.0f}, 5, Emission, {.color = Pink, .shape = ParticleShape::Square});
    particles.Emit(
        {50.0f, 50.0f}, 5, Emission,
        {.color = Pink, .shape = ParticleShape::Spark, .blend = ParticleBlend::Additive, .sparkTrailSeconds = 0.05f});
    particles.Update(0.1f);
    PaintHarness h;
    h.Begin();
    particles.Draw(h.paint);
    h.End();
    REQUIRE(h.Draws().size() == 1);
    CHECK(h.Drawn().size() >= 10 * 6);
}

TEST_CASE("Engine.Fx.CameraEffectsMoveTheViewAndSettle", "[engine][fx]") {
    const CameraView base{.offset = {200.0f, 100.0f}, .target = {200.0f, 100.0f}};
    CameraEffects2D camera(Camera);
    const CameraView still = camera.Apply(base, 1.0f);
    CHECK(still.offset == base.offset);
    CHECK(still.zoom == 1.0f);

    camera.Kick({1.0f, 0.0f});
    camera.Update(1.0f / 60.0f);
    CHECK(camera.Apply(base, 1.0f).offset.x > base.offset.x);
    for (int i = 0; i < 300; ++i)
        camera.Update(1.0f / 60.0f);
    CHECK(camera.Apply(base, 1.0f).offset.x == Approx(base.offset.x).margin(0.01f));

    // A punch zooms in around its focus: the focus stays where it was on screen.
    const Float2 focus{260.0f, 120.0f};
    camera.Punch(0.2f, focus);
    const CameraView punched = camera.Apply(base, 0.0f);
    CHECK(punched.zoom > 1.0f);
    CHECK(punched.ToScreen(focus).x == Approx(base.ToScreen(focus).x).margin(0.05f));
    CHECK(punched.ToScreen(focus).y == Approx(base.ToScreen(focus).y).margin(0.05f));

    CameraEffects2D calm(Camera);
    calm.SetQuality({.shake = EffectLevel::Off});
    calm.Shake(0.5f);
    CHECK(calm.Apply(base, 0.3f).offset == base.offset);

    CameraEffects2D steady(Camera);
    steady.SetQuality({.motion = false});
    steady.SetMotion(1.0f, {1.0f, 0.0f});
    steady.Update(0.5f);
    CHECK(steady.MotionIntensity() == 0.0f);
}

TEST_CASE("Engine.Fx.ParticleRotationAndSpinUseClockwiseRadians", "[engine][fx]") {
    ParticleSystem2D particles({.capacity = 1, .seed = 7U});
    float rotation = -1.0f;
    particles.Emit({50.0f, 50.0f}, 1,
                   {.lifetime = {2.0f, 2.0f},
                    .size = {2.0f, 2.0f},
                    .rotation = {Radians(15.0f), Radians(15.0f)},
                    .spin = {Radians(30.0f), Radians(30.0f)}},
                   {.color = Pink, .draw = [&](Painter& paint, const ParticleDrawData& data) {
                        rotation = data.rotation;
                        DrawParticleShape(paint, ParticleShape::Square, data);
                    }});
    particles.Update(0.5f);
    PaintHarness h({400.0f, 200.0f}, 1.0f);
    h.Begin();
    particles.Draw(h.paint);
    h.End();
    CHECK(rotation == Approx(Pi / 6.0f));
    REQUIRE_FALSE(h.Draws().empty());
    const auto vertices = h.Drawn();
    // The top-right diagonal turns clockwise by 30 degrees, independent of the anti-aliasing margin.
    CHECK(std::any_of(vertices.begin(), vertices.end(), [](const auto& vertex) {
        return vertex.x > 50.0f && vertex.y < 50.0f &&
               (50.0f - vertex.y) / (vertex.x - 50.0f) == Approx(2.0f - std::sqrt(3.0f));
    }));
}

TEST_CASE("Engine.Fx.CameraShotRotationUsesClockwiseRadiansAndWeight", "[engine][fx]") {
    static_assert(Radians(180.0f) == Pi);
    static_assert(Radians(-180.0f) == -Pi);
    const CameraShot shot{.focus = Area.Center(), .rotation = Radians(180.0f), .weight = 0.5f};
    const CameraView camera = ShotCamera(shot, Area, Area);
    CHECK(camera.rotation == Approx(Pi / 2.0f));
    const Float2 right = camera.ToScreen(Area.Center() + Float2{10.0f, 0.0f});
    CHECK(right.x == Approx(Area.Center().x));
    CHECK(right.y == Approx(Area.Center().y + 10.0f));
    CHECK(ShotCamera({.rotation = Radians(90.0f)}, Area, Area).rotation == 0.0f);
    CHECK(BlendShots({}, shot, 0.5f).rotation == Approx(Pi / 2.0f));
}

TEST_CASE("Engine.Fx.CameraShotsFrameTheirBoundsInsideTheSafeArea", "[engine][fx]") {
    const CameraFraming framing{.viewport = Area,
                                .safeArea = {0.0f, 40.0f, 400.0f, 120.0f},
                                .padding = 10.0f,
                                .minZoom = 1.0f,
                                .maxZoom = 4.0f};
    const FloatRect target{300.0f, 20.0f, 40.0f, 20.0f};
    const CameraShot shot = FrameCameraShot(target, framing);
    REQUIRE(shot.weight == 1.0f);
    const CameraView view = ShotCamera(shot, Area, Area);
    const Float2 middle = view.ToScreen(target.Center());
    CHECK(middle.x == Approx(200.0f));
    CHECK(middle.y == Approx(100.0f)); // Centre of the safe area
    CHECK(view.ToScreen(target.Position()).y >= 50.0f - 1e-3f);

    CHECK(FrameCameraShot({}, framing).weight == 0.0f);
    const CameraView neutral = ShotCamera({}, Area, Area);
    CHECK(neutral.ToScreen({37.0f, 81.0f}).x == Approx(37.0f));
    // Close-ups kept in the arena never show outside it.
    const CameraView kept = ShotCamera({.focus = {0.0f, 0.0f}, .zoom = 2.0f, .weight = 1.0f}, Area, Area);
    CHECK(kept.ToWorld({0.0f, 0.0f}).x >= -1e-3f);
    CHECK(kept.ToWorld({0.0f, 0.0f}).y >= -1e-3f);
}

TEST_CASE("Engine.Fx.StageDrawsItsLayersAndHonoursFlashSetting", "[engine][fx]") {
    FxStage stage(Area, Area, TestFx());
    stage.Burst({100.0f, 100.0f}, Pink, 10);
    stage.Ring({100.0f, 100.0f}, Pink);
    stage.Shockwave({100.0f, 100.0f}, Pink);
    stage.FloatText({100.0f, 100.0f}, "+10", Pink);
    stage.Toast("Combo", Pink);
    stage.Flash(Pink);
    stage.Update(0.1f);

    for (const bool overlay : {false, true}) {
        PaintHarness h({400.0f, 200.0f});
        h.context.font = &TestFont();
        h.Begin();
        if (overlay)
            stage.DrawOverlay(h.paint);
        else
            stage.DrawWorldLayer(h.paint);
        h.End();
        CHECK_FALSE(h.Draws().empty());
    }

    FxStage quiet(Area, Area, TestFx());
    quiet.SetQuality({.flashes = false});
    quiet.Flash(Pink);
    quiet.Glitch(0.5f);
    quiet.Update(0.05f);
    PaintHarness q({400.0f, 200.0f});
    q.Begin();
    quiet.DrawOverlay(q.paint);
    q.End();
    CHECK(q.Draws().empty());
}

TEST_CASE("Engine.Fx.IrisCoversAccordingToItsProgress", "[engine][fx]") {
    const IrisStyle style{.curtain = {0, 0, 0, 255}, .innerRim = Pink, .outerRim = Pink, .rimWidth = 3.0f};
    PaintHarness open;
    open.Begin();
    DrawIrisTransition(open.paint, Area, {200.0f, 100.0f}, 0.0f, style);
    open.End();
    CHECK(open.Draws().empty());

    PaintHarness closed;
    closed.Begin();
    DrawIrisTransition(closed.paint, Area, {200.0f, 100.0f}, 1.0f, style);
    closed.End();
    CHECK(closed.Draws().size() == 1); // A plain curtain over the whole area
}
