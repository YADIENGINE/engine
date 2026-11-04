#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
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
#include "Fx/CameraEffects.h"
#include "Fx/CameraShot.h"
#include "Fx/FxCue.h"
#include "Fx/FxCueSet.h"
#include "Fx/FxStage.h"
#include "Fx/ParticleSystem.h"
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
    stage.Update(0.25f);
    CHECK(WorldVertices(stage) == 0); // The burst is still waiting
    stage.Update(0.0f);               // Frozen time, as in a hitstop, moves nothing
    CHECK(WorldVertices(stage) == 0);
    stage.Update(0.25f);
    CHECK(WorldVertices(stage) > 0);

    stage.Clear();
    stage.Play("late", {.at = {100.0f, 100.0f}, .color = Pink});
    stage.Clear(); // Forgets what was waiting too
    stage.Update(1.0f);
    CHECK(WorldVertices(stage) == 0);
}

TEST_CASE("Engine.FxStage.DefaultCuesWorkWithAnyStageAndTheTheme", "[engine][fx]") {
    for (const auto name : DefaultFxCues().Names()) {
        CHECK_FALSE(DefaultFxCues().Get(name).UsesTime());
        FxStage stage = MakeStage();
        stage.Play(name, {.at = {100.0f, 100.0f}, .color = Pink});
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
        for (int i = 0; i < 200; ++i)
            stage.Update(0.01f); // Full intensity
        stage.Update(seconds);
        PaintHarness h({400.0f, 200.0f});
        h.Begin();
        stage.DrawWorldLayer(h.paint);
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

TEST_CASE("Engine.Fx.ParticlesRepeatForTheSameSeedAndDefaultSeed", "[engine][fx]") {
    const auto positions = [](std::optional<std::uint64_t> seed) {
        ParticleSystem particles(seed ? ParticleSystemSettings{.capacity = 8, .seed = *seed}
                                      : ParticleSystemSettings{.capacity = 8});
        std::vector<Float2> seen;
        particles.Emit({10.0f, 10.0f}, 8, {.speed = {10.0f, 90.0f}, .lifetime = {1.0f, 2.0f}, .size = {1.0f, 2.0f}}, {},
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
        static constexpr auto Fields() { return PoseFields(PoseField("arm", &Pose::arm)); }
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
