#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
#include "Animation/Animator.h"
#include "Animation/Clip.h"
#include "Animation/ClipSet.h"
#include "Animation/Tween.h"
#include "Fx/Camera/CameraEffects.h"
#include "Fx/Camera/CameraShot.h"
#include "Fx/FxCue.h"
#include "Fx/FxCueSet.h"
#include "Fx/FxStage.h"
#include "Fx/Particles/ParticleSystem.h"
#include "Fx/Screen/Announcements.h"
#include "Fx/Screen/FlashLimit.h"
#include "Fx/Screen/ScreenFlash.h"
#include "Fx/World/FlashLights.h"
#include "Fx/World/FloatingTexts.h"
#include "Graphics/Style/NeonArcadeTheme.h"
#include "Graphics/Text/FontFace.h"
#include "Runtime/TimeScale.h"
#include "Scene/TransitionClock.h"
#include "Tests/PaintHarness.h"

using Catch::Approx;

namespace {
constexpr Color4 Pink{255, 60, 180, 255};
constexpr FloatRect Area{0.0f, 0.0f, 400.0f, 200.0f};

const FontFace& TestFont() {
    static const FontFace font = FontFace::FromFile(ENGINE_TEST_FONT);
    return font;
}

// How many vertices a layer of the stage makes right now.
std::size_t Vertices(const PaintHarness& h) {
    std::size_t total = 0;
    for (std::size_t i = 0; i < h.Draws().size(); ++i)
        total += h.Drawn(i).size();
    return total;
}
std::size_t WorldVertices(const FxStage& stage) {
    PaintHarness h({400.0f, 200.0f});
    h.context.font = &TestFont();
    h.Begin();
    stage.DrawWorldLayer(h.paint);
    h.End();
    return Vertices(h);
}
std::size_t OverlayVertices(const FxStage& stage) {
    PaintHarness h({400.0f, 200.0f});
    h.context.font = &TestFont();
    h.Begin();
    stage.DrawOverlay(h.paint);
    h.End();
    return Vertices(h);
}
// The brightest vertex: the outermost ones of a shape are its transparent soft edge.
PaintVertex Strongest(const PaintHarness& h) {
    const auto vertices = h.Drawn();
    REQUIRE_FALSE(vertices.empty());
    return *std::ranges::max_element(vertices, {}, [](const PaintVertex& vertex) { return vertex.color.r; });
}
FxStage MakeStage() { return FxStage(Area, Area, NeonArcadeFx); }
} // namespace

TEST_CASE("Engine.FxCue.ChecksItsDataWhenMade", "[engine][fx]") {
    CHECK_THROWS_AS(FxCue(std::vector<FxCueEntry>{}), std::invalid_argument);
    CHECK_THROWS_AS(FxCue({FxShake{0.0f}}), std::invalid_argument);
    CHECK_THROWS_AS(FxCue({FxBurst{ParticleRecipe::Soft, -1}}), std::invalid_argument);
    CHECK_THROWS_AS(FxCue({{-1.0f, FxRing{}}}), std::invalid_argument);
    CHECK_THROWS_AS(FxCue({FxSlowMotion{1.0f, -0.5f}}), std::invalid_argument);
    CHECK_THROWS_AS(FxCue({FxParticles{{ParticleLayer{.share = 1.0f}, ParticleLayer{}}, 5}}), std::invalid_argument);

    const FxCue cue{{0.2f, FxRing{}}, FxFlash{}, {0.1f, FxGlitch{0.5f}}};
    REQUIRE(cue.Entries().size() == 3);
    CHECK(cue.Entries()[0].at == 0.0f); // In order of time
    CHECK(cue.Entries()[2].at == 0.2f);
    CHECK_FALSE(cue.UsesTime());
    CHECK(FxCue{FxHitstop{0.1f}}.UsesTime());
}

TEST_CASE("Engine.FxCueSet.FindsByNameAndRefusesWhatIsWrong", "[engine][fx]") {
    const FxCueSet cues{{"hit", {FxRing{}}}, {"boom", {FxFlash{}}}};
    CHECK(cues.Contains("hit"));
    CHECK_FALSE(cues.Contains("Hit"));
    CHECK(cues.Get("boom").Entries().size() == 1);
    CHECK_THROWS_AS(cues.Get("missing"), std::invalid_argument);
    CHECK(cues.Names().size() == 2);

    CHECK_THROWS_AS(FxCueSet(std::vector<FxCueSetEntry>{}), std::invalid_argument);
    CHECK_THROWS_AS((FxCueSet{{"same", {FxRing{}}}, {"same", {FxFlash{}}}}), std::invalid_argument);
    CHECK_THROWS_AS((FxCueSet{{"", {FxRing{}}}}), std::invalid_argument);

    const FxCueSet more = cues.With({{"hit", {FxFlash{}, FxRing{}}}, {"new", {FxRing{}}}});
    CHECK(more.Names().size() == 3);
    CHECK(more.Get("hit").Entries().size() == 2); // Replaced
    CHECK(cues.Get("hit").Entries().size() == 1); // The original is untouched
}

TEST_CASE("Engine.FxStage.PlaysNamedCuesWithTheirDelays", "[engine][fx]") {
    FxStage stage = MakeStage();
    stage.SetCues(FxCueSet{{"late", {{0.5f, FxBurst{ParticleRecipe::Spark, 20}}}}});
    CHECK_THROWS_AS(stage.Play("missing"), std::invalid_argument);

    stage.Play("late", {.at = {100.0f, 100.0f}, .color = Pink});
    CHECK(WorldVertices(stage) == 0);
    stage.UpdateReal(0.25f);
    stage.Update(0.25f);
    CHECK(WorldVertices(stage) == 0); // The burst is still waiting
    stage.Update(0.0f); // Frozen time, as in a hitstop, moves nothing
    CHECK(WorldVertices(stage) == 0);
    stage.UpdateReal(0.25f);
    stage.Update(0.25f);
    CHECK(WorldVertices(stage) > 0);

    stage.Clear();
    stage.Play("late", {.at = {100.0f, 100.0f}, .color = Pink});
    stage.Clear(); // Forgets what was waiting too
    stage.UpdateReal(1.0f);
    stage.Update(1.0f);
    CHECK(WorldVertices(stage) == 0);
}

TEST_CASE("Engine.FxStage.DefaultCuesWorkWithAnyStageAndTheTheme", "[engine][fx]") {
    for (const auto name : DefaultFxCues().Names()) {
        CHECK_FALSE(DefaultFxCues().Get(name).UsesTime());
        FxStage stage = MakeStage();
        stage.Play(name, {.at = {100.0f, 100.0f}, .color = Pink});
        stage.UpdateReal(0.1f);
        stage.Update(0.1f);
        CHECK(WorldVertices(stage) > 0);
    }
}

TEST_CASE("Engine.FxStage.TextAndColoursComeFromTheCueOrThePlay", "[engine][fx]") {
    FxStyle fx = NeonArcadeFx;
    fx.vignetteSize = 0.0f;
    FxStage stage(Area, Area, fx);
    stage.SetCues(FxCueSet{{"own", {FxFloatText{"fixed"}}},
                           {"given", {FxPopText{20.0f}}},
                           {"white", {{FxFlash{}, Color4{255, 255, 255, 255}}}}});
    stage.Play("given", {.at = {50.0f, 50.0f}, .color = Pink, .text = "+10"});
    stage.UpdateReal(0.1f);
    stage.Update(0.1f); // A popping text starts at size 0
    CHECK(WorldVertices(stage) > 0);
    stage.Clear();
    stage.Play("own", {.at = {50.0f, 50.0f}, .color = Pink});
    CHECK(WorldVertices(stage) > 0);
    stage.Clear();
    stage.Play("white", {.color = Pink});
    PaintHarness h({400.0f, 200.0f});
    h.Begin();
    stage.DrawOverlay(h.paint);
    h.End();
    const PaintVertex flash = Strongest(h);
    CHECK(flash.color.a > 0);
    CHECK(flash.color.g == flash.color.r); // White, not the play's pink
}

TEST_CASE("Engine.FxStage.CuesThatChangeTimeNeedATimeScale", "[engine][fx]") {
    FxStage stage = MakeStage();
    stage.SetCues(FxCueSet{{"freeze", {FxRing{}, FxHitstop{0.1f}}}, {"slow", {FxSlowMotion{1.0f, 0.5f, 0.5f}}}});
    CHECK_THROWS_AS(stage.Play("freeze"), std::invalid_argument);
    CHECK(WorldVertices(stage) == 0); // Refused before anything played

    TimeScale time;
    stage.SetTime(&time);
    stage.Play("freeze", {.color = Pink});
    CHECK(time.Advance(0.05f) == 0.0f);
    time.Clear();

    stage.Play("slow", {.color = Pink});
    CHECK(time.Advance(0.1f) == Approx(0.05f)); // Time slows ...
    stage.UpdateReal(0.05f);
    stage.Update(0.05f);
    CHECK(OverlayVertices(stage) > 0); // ... and it shows
}

TEST_CASE("Engine.FxStage.HonoursThePlayersChoiceOnSlowMotion", "[engine][fx]") {
    FxStyle fx = NeonArcadeFx;
    fx.vignetteSize = 0.0f;
    const auto drawn = [&](const FxQuality& quality) {
        FxStage stage(Area, Area, fx);
        stage.SetQuality(quality);
        stage.SlowMotionLook(1.0f, Pink);
        stage.UpdateReal(0.1f);
        stage.Update(0.1f);
        return OverlayVertices(stage);
    };
    const std::size_t all = drawn({});
    const std::size_t noMotion = drawn({.motion = false});
    const std::size_t noFlashes = drawn({.flashes = false});
    CHECK(all > 0);
    CHECK(noMotion < all); // No speed lines
    CHECK(noFlashes < all); // No tint or dark edges
    CHECK(drawn({.motion = false, .flashes = false}) == 0);
    CHECK_THROWS_AS(FxStage(Area, Area, fx).SlowMotionLook(0.0f, Pink), std::invalid_argument);
}

TEST_CASE("Engine.FxStage.PulseKeepsItsGlowButNotItsBeatWithoutFlashes", "[engine][fx]") {
    const auto alphaAt = [](const FxQuality& quality, float seconds) {
        FxStyle fx = NeonArcadeFx;
        FxStage stage(Area, Area, fx);
        stage.SetQuality(quality);
        stage.SetPulse(true, Pink);
        for (int i = 0; i < 200; ++i) {
            stage.UpdateReal(0.01f);
            stage.Update(0.01f); // Full intensity
        }
        stage.UpdateReal(seconds);
        stage.Update(seconds);
        PaintHarness h({400.0f, 200.0f});
        h.Begin();
        stage.DrawOverlay(h.paint);
        h.End();
        return Strongest(h).color.r; // Additive: premultiplied colour, alpha 0
    };
    CHECK(alphaAt({}, 0.01f) != alphaAt({}, 0.15f));
    CHECK(alphaAt({.flashes = false}, 0.01f) == alphaAt({.flashes = false}, 0.15f));
}

TEST_CASE("Engine.Fx.EventEffectsRefuseInvalidStyles", "[engine][fx]") {
    Shockwaves waves;
    CHECK_THROWS_AS(waves.Emit({}, Pink, {.duration = 0.0f}), std::invalid_argument);
    CHECK_THROWS_AS(waves.Emit({}, Pink, {.duration = 1.0f}, -1.0f), std::invalid_argument);
    ScreenGlitch glitch;
    CHECK_THROWS_AS(glitch.Trigger(0.0f, {}), std::invalid_argument);
    CHECK_THROWS_AS(glitch.Trigger(1.0f, {.fps = 0.0f}), std::invalid_argument);
    ScreenFlash flash;
    CHECK_THROWS_AS(flash.Flash(Pink, {.duration = 0.0f}), std::invalid_argument);
    CHECK_THROWS_AS(PulseOverlay({.period = 0.0f}), std::invalid_argument);
    TimeScale time;
    CHECK_THROWS_AS(time.Hitstop(-1.0f), std::invalid_argument);
    CHECK_THROWS_AS(time.SlowMo(1.0f, -0.5f), std::invalid_argument);
}

TEST_CASE("Engine.FxStage.RefusesAnInvalidStyleWhenMade", "[engine][fx]") {
    CHECK_NOTHROW(FxStage(Area, Area, NeonArcadeFx));
    CHECK_NOTHROW(FxStage(Area, Area, FxStyle{}));
    const auto refuses = [](FxStyle fx, std::string_view part) {
        try {
            FxStage stage(Area, Area, fx);
        } catch (const std::invalid_argument& error) {
            return std::string_view(error.what()).find(part) != std::string_view::npos;
        }
        return false;
    };
    FxStyle fx = NeonArcadeFx;
    fx.ring.duration = 0.0f;
    CHECK(refuses(fx, "ring")); // Names the part, before any ring is asked for
    fx = NeonArcadeFx;
    fx.toast.queuedDuration = -1.0f;
    CHECK(refuses(fx, "toast"));
    fx = NeonArcadeFx;
    fx.particles.debris[0].emission.lifetime = {0.0f, 1.0f};
    CHECK(refuses(fx, "particles"));
    fx = NeonArcadeFx;
    fx.speedLines.count = -1;
    CHECK(refuses(fx, "speedLines"));
    fx = NeonArcadeFx;
    fx.camera.shakeDecay = 0.0f; // It would never settle
    CHECK(refuses(fx, "camera"));
}

TEST_CASE("Engine.FxStage.SlowMotionsLookEndsWithSlowMotion", "[engine][fx]") {
    FxStyle fx = NeonArcadeFx;
    fx.vignetteSize = 0.0f;
    FxStage stage(Area, Area, fx);
    TimeScale time;
    stage.SetTime(&time);
    stage.SetCues(FxCueSet{{"slow", {FxSlowMotion{2.0f, 0.1f, 0.1f}}}});
    stage.Play("slow", {.color = Pink});
    const auto step = [&](int frames) {
        for (int i = 0; i < frames; ++i) {
            const float real = 0.05f;
            stage.UpdateReal(real);
            stage.Update(time.Advance(real));
        }
    };
    step(30); // 1.5 of 2 real seconds
    CHECK(OverlayVertices(stage) > 0);
    step(11);
    CHECK(time.Advance(0.05f) == 0.05f);
    CHECK(OverlayVertices(stage) == 0); // Over when time is back to normal, though little game time has passed
}

TEST_CASE("Engine.FxStage.AnnouncementsKeepRealTime", "[engine][fx]") {
    FxStyle fx = NeonArcadeFx;
    fx.vignetteSize = 0.0f;
    FxStage stage(Area, Area, fx);
    stage.Toast("Combo", Pink);
    stage.Banner("Round 2");
    CHECK(OverlayVertices(stage) > 0);
    stage.UpdateReal(0.5f); // A hitstop: nothing else moves
    CHECK(OverlayVertices(stage) > 0);
    stage.UpdateReal(2.0f);
    CHECK(OverlayVertices(stage) == 0);
}

TEST_CASE("Engine.Fx.ToastsWaitTheirTurn", "[engine][fx]") {
    const ToastStyle style{.duration = 1.0f, .queuedDuration = 0.25f, .text = {.size = 20.0f}};
    const auto seen = [](const Announcements& announcements) {
        PaintHarness h({400.0f, 200.0f});
        h.context.font = &TestFont();
        h.Begin();
        announcements.Draw(h.paint, Area);
        h.End();
        return Vertices(h);
    };
    Announcements toasts(1);
    toasts.Toast("a", Pink, style);
    const std::size_t one = seen(toasts);
    CHECK(one > 0);
    toasts.Toast("bb", Pink, style);  // Shortens the first and waits
    toasts.Toast("ccc", Pink, style); // A full queue drops its oldest: "bb"
    CHECK(seen(toasts) == one);
    toasts.Update(0.3f);
    CHECK(seen(toasts) > one); // "ccc", with nothing behind it, stays its full duration
    toasts.Update(0.9f);
    CHECK(seen(toasts) > 0);
    toasts.Update(0.2f);
    CHECK(seen(toasts) == 0);

    CHECK_THROWS_AS(toasts.Toast("x", Pink, {.duration = 0.0f}), std::invalid_argument);
    CHECK_THROWS_AS(toasts.Banner("x", {.fadeSeconds = -1.0f}), std::invalid_argument);
}

TEST_CASE("Engine.Fx.FloatingTextsMoveAndEnd", "[engine][fx]") {
    FloatingTexts texts;
    texts.Emit({100.0f, 100.0f}, "+1", Pink,
               {.duration = 1.0f, .velocity = {0.0f, -50.0f}, .text = {.size = 20.0f, .anchor = {0.0f, 0.0f}}});
    const auto top = [&] {
        PaintHarness h({400.0f, 200.0f});
        h.context.font = &TestFont();
        h.Begin();
        texts.Draw(h.paint);
        h.End();
        return Vertices(h);
    };
    CHECK(top() > 0);
    texts.Update(0.5f);
    CHECK(top() > 0);
    texts.Update(0.6f);
    CHECK(top() == 0);
    CHECK_THROWS_AS(texts.Emit({}, "x", Pink, {.duration = -1.0f}), std::invalid_argument);
    CHECK_THROWS_AS(texts.Emit({}, "x", Pink, {.text = {.size = 0.0f}}), std::invalid_argument);
}

TEST_CASE("Engine.FxStage.AWaitingActionThatThrowsThrowsOnce", "[engine][fx]") {
    FxStage stage = MakeStage();
    TimeScale time;
    stage.SetTime(&time);
    stage.SetCues(FxCueSet{{"late", {{0.1f, FxHitstop{0.1f}}}}});
    stage.Play("late");
    stage.SetTime(nullptr);
    CHECK_THROWS_AS(stage.Update(0.2f), std::invalid_argument);
    CHECK_NOTHROW(stage.Update(0.2f));
}

TEST_CASE("Engine.Fx.FullParticlesMakeRoomForNewOnes", "[engine][fx]") {
    ParticleSystem particles({.capacity = 10});
    const ParticleEmission still{.lifetime = {5.0f, 5.0f}, .size = {1.0f, 1.0f}};
    particles.Emit({1.0f, 0.0f}, Pink, 10, still, {});
    particles.Emit({2.0f, 0.0f}, Pink, 4, still, {});
    CHECK(particles.ActiveCount() == 10);
    int fresh = 0;
    particles.Emit({3.0f, 0.0f}, Pink, 2, still, {}, [&](Painter&, const ParticleDrawData&) { ++fresh; });
    PaintHarness h;
    h.Begin();
    particles.Draw(h.paint);
    h.End();
    CHECK(fresh == 2);
    CHECK(particles.ActiveCount() == 10);
}

TEST_CASE("Engine.Fx.ABrighterFlashWinsAndFlashesAreRationed", "[engine][fx]") {
    const auto strength = [](const ScreenFlash& flash) {
        PaintHarness h({400.0f, 200.0f});
        h.Begin();
        flash.Draw(h.paint, Area);
        h.End();
        return h.Draws().empty() ? 0 : static_cast<int>(Strongest(h).color.a);
    };
    ScreenFlash flash;
    const FlashStyle strong{.duration = 2.0f, .opacity = 1.0f};
    const FlashStyle weak{.duration = 2.0f, .opacity = 0.2f};
    flash.Flash(Pink, strong);
    const int bright = strength(flash);
    flash.Update(ScreenFlash::MinSecondsBetween + 0.01f);
    flash.Flash(Pink, weak); // The first is still brighter
    CHECK(strength(flash) > 100);

    ScreenFlash rationed;
    rationed.Flash(Pink, weak);
    const int dim = strength(rationed);
    rationed.Flash(Pink, strong); // Too soon, however bright
    CHECK(strength(rationed) == dim);
    rationed.Update(ScreenFlash::MinSecondsBetween + 0.01f);
    rationed.Flash(Pink, strong);
    CHECK(strength(rationed) == bright);
}

TEST_CASE("Engine.FxStage.ALightFlashIsAtItsPlaceOrOverTheWholeScene", "[engine][fx]") {
    FxStage stage = MakeStage();
    stage.Play(FxCue{FxLightFlash{.radius = 300.0f, .seconds = 1.0f}}, {.at = {50.0f, 60.0f}, .color = Pink});
    REQUIRE(stage.LightFlashes().lights.size() == 1);
    const LightSource light = stage.LightFlashes().lights[0];
    CHECK(light.at == Float2{50.0f, 60.0f});
    CHECK(light.radius == Approx(300.0f));
    CHECK(light.color == Pink);
    CHECK(light.intensity == Approx(1.0f));
    CHECK(stage.LightFlashes().ambient == Color4{0, 0, 0, 255});

    stage.Clear();
    stage.Play(FxCue{FxLightFlash{.seconds = 1.0f, .strength = 0.5f}}, {.color = {200, 100, 0, 255}});
    CHECK(stage.LightFlashes().lights.empty());
    CHECK(stage.LightFlashes().ambient == Color4{100, 50, 0, 255});
}

TEST_CASE("Engine.FxStage.ALightFlashFadesOnGameTimeAndEnds", "[engine][fx]") {
    FxStage stage = MakeStage();
    stage.LightFlash({}, Pink, {.radius = 100.0f, .seconds = 1.0f});
    stage.UpdateReal(0.5f); // A hitstop holds it, as real time goes on
    CHECK(stage.LightFlashes().lights[0].intensity == Approx(1.0f));
    stage.UpdateReal(0.5f);
    stage.Update(0.5f);
    CHECK(stage.LightFlashes().lights[0].intensity == Approx(0.25f)); // Falls away quickly
    stage.UpdateReal(0.5f);
    stage.Update(0.5f);
    CHECK(stage.LightFlashes().lights.empty());
}

TEST_CASE("Engine.FxStage.IntensityScalesALightFlashesStrengthNotItsTime", "[engine][fx]") {
    FxStage stage = MakeStage();
    stage.Play(FxCue{FxLightFlash{.radius = 100.0f, .seconds = 1.0f}}, {.intensity = 0.5f});
    REQUIRE(stage.LightFlashes().lights.size() == 1);
    CHECK(stage.LightFlashes().lights[0].intensity == Approx(0.5f));
    CHECK(stage.LightFlashes().lights[0].radius == Approx(100.0f));
    stage.UpdateReal(0.999f);
    stage.Update(0.999f);
    CHECK(stage.LightFlashes().lights.size() == 1);
}

TEST_CASE("Engine.FxStage.WithFlashesOffThereAreNoLightFlashes", "[engine][fx]") {
    FxStage stage = MakeStage();
    stage.Play(FxCue{FxLightFlash{.radius = 100.0f}}, {.color = Pink});
    REQUIRE(stage.LightFlashes().lights.size() == 1);
    stage.SetQuality({.flashes = false}); // One showing goes with the choice
    CHECK(stage.LightFlashes().lights.empty());
    stage.UpdateReal(1.0f);
    stage.Update(1.0f);
    stage.Play(FxCue{FxLightFlash{.radius = 100.0f}}, {.color = Pink});
    stage.Play(FxCue{FxLightFlash{.seconds = 1.0f}}, {.color = Pink});
    CHECK(stage.LightFlashes().lights.empty());
    CHECK(stage.LightFlashes().ambient == Color4{0, 0, 0, 255});
}

TEST_CASE("Engine.FxStage.LightFlashesAreRationedWithScreenFlashes", "[engine][fx]") {
    const LightFlashStyle flash{.radius = 100.0f, .seconds = 5.0f};
    FxStage stage = MakeStage();
    stage.LightFlash({}, Pink, flash);
    stage.LightFlash({}, Pink, flash); // Too soon
    CHECK(stage.LightFlashes().lights.size() == 1);
    stage.UpdateReal(0.1f);
    stage.Update(0.1f);
    stage.LightFlash({}, Pink, flash);
    CHECK(stage.LightFlashes().lights.size() == 1);
    stage.UpdateReal(FlashLimit::MinSecondsBetween);
    stage.Update(FlashLimit::MinSecondsBetween);
    stage.LightFlash({}, Pink, flash);
    CHECK(stage.LightFlashes().lights.size() == 2);

    // A screen flash counts, or the two taking turns would double the limit.
    FxStage turns = MakeStage();
    turns.Flash(Pink);
    turns.Update(0.1f);
    turns.LightFlash({}, Pink, flash);
    CHECK(turns.LightFlashes().lights.empty());

    // Together they are one flash.
    FxStage together = MakeStage();
    together.Play(FxCue{FxFlash{}, FxLightFlash{flash}}, {.color = Pink});
    CHECK(together.LightFlashes().lights.size() == 1);
    CHECK(OverlayVertices(together) > 0);
}

TEST_CASE("Engine.Fx.FlashLightsKeepTheNewestAndRefuseWhatIsWrong", "[engine][fx]") {
    FlashLights lights;
    FlashLimit limit;
    for (std::size_t i = 0; i <= FlashLights::MaxPlaying; ++i) {
        lights.Start(limit, {static_cast<float>(i), 0.0f}, Pink, {.radius = 10.0f, .seconds = 100.0f});
        limit.Update(FlashLimit::MinSecondsBetween + 0.01f);
    }
    REQUIRE(lights.Frame().lights.size() == FlashLights::MaxPlaying);
    CHECK(lights.Frame().lights[0].at.x == Approx(1.0f));
    CHECK_THROWS_AS(lights.Start(limit, {}, Pink, {.seconds = 0.0f}), std::invalid_argument);
    CHECK_THROWS_AS(FxCue{FxLightFlash{.radius = -1.0f}}, std::invalid_argument);
}

TEST_CASE("Engine.FxStage.FlashesOffAlsoCoversWavesAndCurtains", "[engine][fx]") {
    FxStyle fx = NeonArcadeFx;
    fx.vignetteSize = 0.0f;
    FxStage stage(Area, Area, fx);
    stage.SetQuality({.flashes = false});
    stage.Shockwave({100.0f, 100.0f}, Pink);
    stage.UpdateReal(0.1f);
    stage.Update(0.1f);
    CHECK(WorldVertices(stage) == 0);

    Announcements plain(1);
    plain.SetQuality({.flashes = false});
    Announcements full(1);
    const BannerStyle banner{
        .curtainFadeSeconds = 1.0f, .curtain = {0, 0, 0, 255}, .text = {.size = 20.0f}};
    plain.Banner("Round 2", banner);
    full.Banner("Round 2", banner);
    const auto vertices = [](const Announcements& announcements) {
        PaintHarness h({400.0f, 200.0f});
        h.context.font = &TestFont();
        h.Begin();
        announcements.Draw(h.paint, Area);
        h.End();
        return Vertices(h);
    };
    CHECK(vertices(plain) > 0);
    CHECK(vertices(plain) < vertices(full));
}

TEST_CASE("Engine.Fx.ShakeBuildsWithItsAmountAndSettles", "[engine][fx]") {
    const CameraEffectsStyle style{
        .shakeOffset = 10.0f, .shakeAngle = 0.1f, .shakeSpeed = 20.0f, .shakeDecay = 2.0f, .maxStep = 1.0f};
    const CameraView base{.offset = {200.0f, 100.0f}, .target = {200.0f, 100.0f}};
    const auto widest = [&](const CameraEffects& camera) {
        float offset = 0.0f;
        float turn = 0.0f;
        for (int i = 0; i < 200; ++i) {
            const CameraView view = camera.Apply(base, static_cast<float>(i) * 0.013f);
            offset = std::max(offset, std::abs(view.offset.x - base.offset.x));
            turn = std::max(turn, std::abs(view.rotation));
        }
        return std::pair{offset, turn};
    };
    CameraEffects small(style);
    small.Shake(0.3f);
    CameraEffects large(style);
    large.Shake(0.9f);
    CHECK(widest(large).first > widest(small).first * 4.0f); // The square of the amount
    CHECK(widest(large).first <= 10.0f);
    CHECK(widest(large).second > 0.0f);

    small.Shake(0.3f);
    CHECK(small.ShakeAmount() == Approx(0.6f)); // Hits in a row build up ...
    small.Shake(5.0f);
    CHECK(small.ShakeAmount() == 1.0f); // ... to a limit

    const auto after = [&](int frames) {
        CameraEffects camera(style);
        camera.Shake(1.0f);
        for (int i = 0; i < frames; ++i)
            camera.Update(0.25f / static_cast<float>(frames));
        return camera.ShakeAmount();
    };
    CHECK(after(5) == Approx(after(60)));
    large.Update(1.0f);
    CHECK(large.ShakeAmount() == 0.0f);
    CHECK(large.Apply(base, 1.0f).offset == base.offset);
    CHECK(large.Apply(base, 1.0f).rotation == 0.0f);

    CameraEffects calm(style);
    calm.SetQuality({.shake = EffectLevel::Off});
    calm.Shake(1.0f);
    CHECK(calm.Apply(base, 0.3f).rotation == 0.0f);

    CHECK_THROWS_AS(calm.Shake(-1.0f), std::invalid_argument);
    CHECK_THROWS_AS(calm.Kick({std::nanf(""), 0.0f}), std::invalid_argument);
    CHECK_THROWS_AS(calm.SetMotion(std::nanf(""), {}), std::invalid_argument);
}

TEST_CASE("Engine.FxStage.WorldCameraWorksOnAGamesOwnCamera", "[engine][fx]") {
    FxStage stage = MakeStage();
    PaintHarness h({400.0f, 200.0f});
    h.Begin();
    // A camera that has scrolled far from the stage's area.
    const CameraView follow{.offset = {200.0f, 100.0f}, .target = {3000.0f, 900.0f}, .zoom = 2.0f};
    const CameraView still = stage.WorldCamera(h.paint, follow);
    CHECK(still.target == follow.target);
    CHECK(still.offset == follow.offset);
    CHECK(still.zoom == follow.zoom);

    stage.Kick({1.0f, 0.0f});
    stage.UpdateReal(1.0f / 60.0f);
    stage.Update(1.0f / 60.0f);
    CHECK(stage.WorldCamera(h.paint, follow).offset.x > follow.offset.x);
    CHECK(stage.WorldCamera(h.paint, follow).target == follow.target);
    stage.Clear();

    stage.SetShot({.focus = {3100.0f, 900.0f}, .zoom = 4.0f, .weight = 0.5f});
    const CameraView blended = stage.WorldCamera(h.paint, follow);
    CHECK(blended.target.x == Approx(3050.0f)); // Halfway, not held inside the stage's area
    CHECK(blended.zoom == Approx(3.0f));
    h.End();
}

TEST_CASE("Engine.FxStage.IntensitySizesOnePlay", "[engine][fx]") {
    const auto particles = [](float intensity) {
        FxStage stage(Area, Area, NeonArcadeFx);
        stage.SetCues(FxCueSet{{"burst", {FxBurst{ParticleRecipe::Spark, 40}}}});
        stage.Play("burst", {.at = {100.0f, 100.0f}, .color = Pink, .intensity = intensity});
        return WorldVertices(stage);
    };
    CHECK(particles(0.25f) < particles(1.0f));
    CHECK(particles(1.0f) < particles(2.0f));
    CHECK(particles(0.01f) > 0); // At least one
    CHECK(particles(0.0f) == 0);

    FxStage stage = MakeStage();
    CHECK_THROWS_AS(stage.Play("hit", {.intensity = -1.0f}), std::invalid_argument);
    CHECK_THROWS_AS(stage.Play("explosion", {.intensity = std::nanf("")}), std::invalid_argument);

    const auto delayed = [](float intensity) {
        FxStage late(Area, Area, NeonArcadeFx);
        late.SetCues(FxCueSet{{"late", {{0.1f, FxBurst{ParticleRecipe::Spark, 40}}}}});
        late.Play("late", {.at = {100.0f, 100.0f}, .color = Pink, .intensity = intensity});
        late.Update(0.11f);
        return WorldVertices(late);
    };
    CHECK(delayed(0.25f) < delayed(1.0f)); // Waiting keeps it
}

TEST_CASE("Engine.FxStage.CooldownLimitsACueByName", "[engine][fx]") {
    FxStage stage = MakeStage();
    stage.SetCues(FxCueSet{{"hit", {FxBurst{ParticleRecipe::Spark, 4}}, 0.1f}, {"free", {FxRing{}}}});
    const FxPlay play{.at = {100.0f, 100.0f}, .color = Pink};
    CHECK(stage.Play("hit", play));
    CHECK_FALSE(stage.Play("hit", play));
    CHECK(stage.Play("free", play)); // Another name, and no cooldown
    CHECK(stage.Play("free", play));
    stage.UpdateReal(1.0f); // A hitstop holds it
    CHECK_FALSE(stage.Play("hit", play));
    stage.UpdateReal(0.11f);
    stage.Update(0.11f);
    CHECK(stage.Play("hit", play));
    stage.Clear();
    CHECK(stage.Play("hit", play));
    CHECK_THROWS_AS(stage.Play("missing"), std::invalid_argument);
    CHECK_THROWS_AS((FxCueSet{{"bad", {FxRing{}}, -1.0f}}), std::invalid_argument);
    CHECK(DefaultFxCues().Cooldown("hit") > 0.0f);
    CHECK(DefaultFxCues().Cooldown("explosion") == 0.0f);
}

TEST_CASE("Engine.Fx.ParticlesRepeatForTheSameSeedAndDefaultSeed", "[engine][fx]") {
    const auto positions = [](std::optional<std::uint64_t> seed) {
        ParticleSystem particles(seed ? ParticleSystemSettings{.capacity = 8, .seed = *seed}
                                      : ParticleSystemSettings{.capacity = 8});
        std::vector<Float2> seen;
        particles.Emit({10.0f, 10.0f}, Pink, 8,
                       {.speed = {10.0f, 90.0f}, .lifetime = {1.0f, 2.0f}, .size = {1.0f, 2.0f}}, {},
                       [&](Painter&, const ParticleDrawData& data) { seen.push_back(data.position); });
        particles.Update(0.1f);
        PaintHarness h;
        h.Begin();
        particles.Draw(h.paint);
        h.End();
        return seen;
    };
    CHECK(positions(std::nullopt) == positions(std::nullopt)); // Frame checks can compare runs
    CHECK(positions(5U) == positions(5U));
    CHECK(positions(5U) != positions(6U));
}

TEST_CASE("Engine.Fx.CameraDecaysDoNotDependOnTheFrameRate", "[engine][fx]") {
    const CameraEffectsStyle style{.punchDecay = 8.0f, .maxStep = 1.0f, .motionBlendSpeed = 6.0f};
    const CameraView base{.offset = {200.0f, 100.0f}, .target = {200.0f, 100.0f}};
    const auto after = [&](int frames) {
        CameraEffects camera(style);
        camera.Punch(1.0f);
        camera.SetMotion(1.0f, {});
        for (int i = 0; i < frames; ++i)
            camera.Update(0.25f / static_cast<float>(frames));
        return std::pair{camera.Apply(base, 0.0f).zoom, camera.MotionIntensity()};
    };
    CHECK(after(5).first == Approx(after(60).first));
    CHECK(after(5).second == Approx(after(60).second));
    CHECK(DecayFactor(8.0f, 0.5f) == Approx(DecayFactor(8.0f, 0.25f) * DecayFactor(8.0f, 0.25f)));
}

TEST_CASE("Engine.Fx.CameraShotIsAnAnimationValue", "[engine][fx]") {
    const CameraShot close{
        .focus = {100.0f, 50.0f}, .zoom = 3.0f, .rotation = 1.0f, .weight = 1.0f, .keepInArena = false};

    SecondsTween<CameraShot> shot{CameraShot{}};
    shot.To(close, 1.0f, Curve::Linear);
    shot.Update(0.5f);
    CHECK(shot.Value().zoom == Approx(2.0f));
    CHECK(shot.Value().weight == Approx(0.5f));
    CHECK(shot.Value().focus.x == Approx(50.0f));
    shot.Update(0.5f);
    CHECK(shot.Value().zoom == Approx(3.0f));
    CHECK_FALSE(shot.Value().keepInArena);

    const SecondsClipSet<CameraShot> moves{
        {"push", SecondsClip<CameraShot>{}
                     .Track(&CameraShot::zoom, {{0.0f, 1.0f}, {1.0f, 2.0f}})
                     .Track(&CameraShot::weight, {{0.0f, 0.0f}, {0.5f, 1.0f}})
                     .Moment("boom", 0.75f)}};
    SecondsAnimator<CameraShot> camera(moves, "push");
    int booms = 0;
    camera.Update(1.0f, [&](std::string_view moment) { booms += moment == "boom" ? 1 : 0; });
    CHECK(camera.Pose().zoom == Approx(2.0f));
    CHECK(camera.Pose().weight == Approx(1.0f));
    CHECK(booms == 1);
}

TEST_CASE("Engine.Fx.AnimationMomentsCanPlayCues", "[engine][fx]") {
    struct Pose {
        float arm = 0.0f;
    };
    const SecondsClipSet<Pose> clips{
        {"attack",
         SecondsClip<Pose>{}.Track(&Pose::arm, {{0.0f, 0.0f}, {1.0f, 1.0f}}).Moment("hit", 0.5f).Moment("sway", 0.6f)}};
    SecondsAnimator<Pose> animator(clips, "attack");
    FxStage stage = MakeStage();
    const auto play = [&](std::string_view moment) {
        if (stage.Cues().Contains(moment)) stage.Play(moment, {.at = {100.0f, 100.0f}, .color = Pink});
    };
    animator.Update(0.25f, play);
    CHECK(WorldVertices(stage) == 0);
    animator.Update(0.5f, play); // Meets "hit", a cue, and "sway", which is not one
    CHECK(WorldVertices(stage) > 0);
}

TEST_CASE("Engine.Time.SlowMotionFollowsItsCurve", "[engine][time]") {
    TimeScale time;
    time.SlowMo(1.0f, 0.0f, 1.0f, Curve::OutCubic);
    CHECK(time.Advance(0.5f) == 0.0f); // Where the frame starts
    CHECK(time.Advance(0.5f) == Approx(0.5f * EaseOutCubic(0.5f)));
    CHECK(time.Advance(0.5f) == 0.5f);
    time.SlowMo(0.0f, 0.25f); // Nothing to do
    CHECK(time.Advance(0.5f) == 0.5f);
}

TEST_CASE("Engine.Transition.CoverClosesHoldsAndOpens", "[engine]") {
    TransitionClock transition(0.5f, 0.2f, 0.5f);
    CHECK(transition.Cover() == 0.0f);
    CHECK_FALSE(transition.Opening());
    transition.Start();
    CHECK(transition.Update(0.5f));
    CHECK(transition.Cover() == 1.0f);
    CHECK_FALSE(transition.Opening());
    CHECK_FALSE(transition.Update(0.2f));
    CHECK(transition.Opening());
    CHECK(transition.Cover() == 1.0f);
    CHECK_FALSE(transition.Update(0.25f));
    CHECK(transition.Cover() == Approx(0.5f));
    CHECK_FALSE(transition.Update(0.25f));
    CHECK_FALSE(transition.Active());
    CHECK(transition.Cover() == 0.0f);
    CHECK_THROWS_AS(TransitionClock(0.0f, 0.0f, 1.0f), std::invalid_argument);
}
