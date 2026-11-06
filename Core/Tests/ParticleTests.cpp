#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <vector>
#include "Fx/FxCue.h"
#include "Fx/FxStage.h"
#include "Fx/Particles/ParticleEmitter.h"
#include "Fx/Particles/ParticleRecipes.h"
#include "Fx/Particles/ParticleSystem.h"
#include "Graphics/Style/NeonArcadeTheme.h"
#include "Tests/PaintHarness.h"
#include "Utilities/Math/Scalar.h"

using Catch::Approx;

namespace {
constexpr Color4 Pink{255, 60, 180, 255};
constexpr FloatRect Area{0.0f, 0.0f, 400.0f, 200.0f};

// Particles that stay where they are born, for checks on where that is.
constexpr ParticleEmission Still{.lifetime = {5.0f, 5.0f}, .size = {1.0f, 1.0f}};

// What one Draw shows, read through a custom drawing.
std::vector<ParticleDrawData> Seen(ParticleSystem& particles, const ParticleSource& from, int count,
                                   const ParticleEmission& emission, const ParticleAppearance& appearance = {},
                                   float seconds = 0.0f) {
    std::vector<ParticleDrawData> seen;
    particles.Emit(from, Pink, count, emission, appearance,
                   [&](Painter&, const ParticleDrawData& data) { seen.push_back(data); });
    particles.Update(seconds);
    PaintHarness h;
    h.Begin();
    particles.Draw(h.paint);
    h.End();
    return seen;
}

// The x range a stream's particles cover, read from the drawn squares.
struct Span {
    float low = 1e9f;
    float high = -1e9f;
};
Span DrawnSpan(const ParticleSystem& particles) {
    PaintHarness h({400.0f, 200.0f}, 1.0f);
    h.Begin();
    particles.Draw(h.paint);
    h.End();
    Span span;
    if (h.Draws().empty()) return span;
    for (const PaintVertex& vertex : h.Drawn()) {
        span.low = std::min(span.low, vertex.x);
        span.high = std::max(span.high, vertex.x);
    }
    return span;
}

ParticleEmitterStyle Stream(float perSecond, float perUnit) {
    return {
        .emission = Still, .appearance = {.shape = ParticleShape::Square}, .perSecond = perSecond, .perUnit = perUnit};
}
} // namespace

TEST_CASE("Engine.Particles.ColorKeysMultiplyTheEmittedColourUnlessTheyAreTheirOwn", "[engine][fx]") {
    const ParticleColors keys{.keys = {{{0.0f, {255, 255, 255, 255}}, {1.0f, {128, 128, 128, 0}}}}, .count = 2};
    const ParticleAppearance tinted{.colors = keys};
    CHECK(ParticleColorAt(tinted, Pink, 0.0f) == Pink);
    const Color4 half = ParticleColorAt(tinted, Pink, 0.5f); // Halfway to grey and clear
    CHECK(half.r == 192);
    CHECK(half.a == 128);
    CHECK(ParticleColorAt(tinted, Pink, 1.0f) == Color4{128, 30, 90, 0});

    const ParticleAppearance own{.colors = keys, .ownColors = true};
    CHECK(ParticleColorAt(own, Pink, 0.0f) == Color4{255, 255, 255, 255});
    CHECK(ParticleColorAt(own, Pink, 2.0f) == Color4{128, 128, 128, 0}); // After the last key, the last

    CHECK(ParticleColorAt({}, Pink, 0.7f) == Pink); // No keys: the emitted colour as it is
    const ParticleAppearance fading{.colors = ParticleFadeOut(0.5f)};
    CHECK(ParticleColorAt(fading, Pink, 0.4f) == Pink);
    CHECK(ParticleColorAt(fading, Pink, 0.75f).a == 128);
    CHECK(ParticleColorAt(fading, Pink, 1.0f).a == 0);
    CHECK(ParticleFadeOut(0.0f).count == 0);
    CHECK(ParticleFadeOut(1.0f).count == 2);
}

TEST_CASE("Engine.Particles.InvalidLooksAndAreasThrow", "[engine][fx]") {
    ParticleSystem particles({.capacity = 4});
    const auto emit = [&](const ParticleEmission& emission, const ParticleAppearance& appearance) {
        particles.Emit({}, Pink, 1, emission, appearance);
    };
    CHECK_THROWS_AS(emit(Still, {.colors = {.count = 9}}), std::invalid_argument);
    CHECK_THROWS_AS(emit(Still, {.colors = {.keys = {{{0.6f, Pink}, {0.2f, Pink}}}, .count = 2}}),
                    std::invalid_argument);                                   // Back in time
    CHECK_THROWS_AS(emit(Still, {.ownColors = true}), std::invalid_argument); // Its own colours, but none
    CHECK_THROWS_AS(emit(Still, {.size = {.end = -1.0f}}), std::invalid_argument);
    ParticleEmission ring = Still;
    ring.area = {.shape = ParticleAreaShape::Ring, .radius = 2.0f, .innerRadius = 3.0f};
    CHECK_THROWS_AS(emit(ring, {}), std::invalid_argument);
    ParticleEmission point = Still;
    point.area.outward = true; // Away from a point is no direction
    CHECK_THROWS_AS(emit(point, {}), std::invalid_argument);
    CHECK(particles.ActiveCount() == 0);
}

TEST_CASE("Engine.Particles.SizeFollowsItsCurveThroughLife", "[engine][fx]") {
    ParticleSystem particles({.capacity = 1});
    const ParticleEmission emission{.lifetime = {2.0f, 2.0f}, .size = {4.0f, 4.0f}};
    const auto seen = Seen(particles, {}, 1, emission, {.size = {.start = 1.0f, .end = 3.0f}}, 1.0f);
    REQUIRE(seen.size() == 1);
    CHECK(seen[0].size == Approx(8.0f)); // Halfway from 4 to 12
    CHECK(seen[0].normalizedAge == Approx(0.5f));
}

TEST_CASE("Engine.Particles.ALooksCustomDrawingIsReleasedWithItsLastParticle", "[engine][fx]") {
    ParticleSystem particles({.capacity = 8});
    auto held = std::make_shared<int>(0);
    const ParticleEmission brief{.lifetime = {0.5f, 0.5f}, .size = {1.0f, 1.0f}};
    particles.Emit({}, Pink, 3, brief, {}, [held](Painter&, const ParticleDrawData&) {});
    particles.Emit({}, Pink, 3, brief, {.shape = ParticleShape::Square}); // A look of its own, shared by three
    CHECK(held.use_count() == 2);
    particles.Update(1.0f);
    CHECK(particles.ActiveCount() == 0);
    CHECK(held.use_count() == 1);
}

TEST_CASE("Engine.Particles.AreasPlaceParticlesInsideTheirShape", "[engine][fx]") {
    ParticleSystem particles({.capacity = 200, .seed = 11U});
    const Float2 centre{100.0f, 100.0f};
    ParticleEmission ring = Still;
    ring.area = {.shape = ParticleAreaShape::Ring, .radius = 20.0f, .innerRadius = 15.0f};
    for (const auto& data : Seen(particles, centre, 100, ring)) {
        const float distance = Distance(centre, data.position);
        CHECK(distance >= 15.0f - 1e-3f);
        CHECK(distance <= 20.0f + 1e-3f);
    }
    particles.Clear();
    ParticleEmission line = Still;
    line.area = {.shape = ParticleAreaShape::Line, .length = 40.0f, .angle = Pi * 0.5f}; // Straight down
    for (const auto& data : Seen(particles, centre, 100, line)) {
        CHECK(data.position.x == Approx(100.0f).margin(1e-3f));
        CHECK(std::abs(data.position.y - 100.0f) <= 20.0f + 1e-3f);
    }
    particles.Clear();
    ParticleEmission box = Still;
    box.area = {.shape = ParticleAreaShape::Box, .halfSize = {30.0f, 5.0f}};
    for (const auto& data : Seen(particles, centre, 100, box)) {
        CHECK(std::abs(data.position.x - 100.0f) <= 30.0f + 1e-3f);
        CHECK(std::abs(data.position.y - 100.0f) <= 5.0f + 1e-3f);
    }
}

TEST_CASE("Engine.Particles.OutwardAreasFlyAwayFromTheirCentre", "[engine][fx]") {
    ParticleSystem particles({.capacity = 50, .seed = 5U});
    ParticleEmission outward{.speed = {10.0f, 10.0f}, .lifetime = {5.0f, 5.0f}, .size = {1.0f, 1.0f}, .spread = 0.0f};
    outward.area = {.shape = ParticleAreaShape::Disc, .radius = 10.0f, .outward = true};
    const Float2 centre{50.0f, 50.0f};
    for (const auto& data : Seen(particles, centre, 50, outward)) {
        const Float2 away = data.position - centre;
        CHECK(Dot(away, data.velocity) > 0.0f);
    }
}

TEST_CASE("Engine.Particles.ParticlesInheritTheirSourcesVelocity", "[engine][fx]") {
    ParticleSystem particles({.capacity = 1});
    ParticleEmission emission = Still;
    emission.inherit = 0.5f;
    const auto seen = Seen(particles, {{10.0f, 10.0f}, {100.0f, -40.0f}}, 1, emission);
    REQUIRE(seen.size() == 1);
    CHECK(seen[0].velocity.x == Approx(50.0f));
    CHECK(seen[0].velocity.y == Approx(-20.0f));
}

TEST_CASE("Engine.Particles.AnEmittersRateDoesNotDependOnTheFrameRate", "[engine][fx]") {
    for (const int frames : {30, 60, 144}) {
        ParticleSystem particles({.capacity = 500});
        ParticleEmitter emitter(Stream(60.0f, 0.0f));
        for (int frame = 0; frame < frames; ++frame)
            emitter.Emit(particles, {10.0f, 10.0f}, Pink, 1.0f / static_cast<float>(frames));
        CHECK(particles.ActiveCount() == 60);
    }
    for (const int steps : {1, 4, 100}) { // 200 units at half a particle each
        ParticleSystem particles({.capacity = 500});
        ParticleEmitter emitter(Stream(0.0f, 0.5f));
        emitter.Emit(particles, {0.0f, 10.0f}, Pink, 0.0f);
        for (int step = 1; step <= steps; ++step)
            emitter.Emit(particles, {200.0f * static_cast<float>(step) / static_cast<float>(steps), 10.0f}, Pink,
                         0.01f);
        CHECK(particles.ActiveCount() == 100);
    }
}

TEST_CASE("Engine.Particles.AnEmitterFillsTheWayAndAJumpDoesNot", "[engine][fx]") {
    ParticleSystem particles({.capacity = 100});
    ParticleEmitter emitter(Stream(0.0f, 0.1f));
    emitter.Emit(particles, {0.0f, 100.0f}, Pink, 0.0f);
    emitter.Emit(particles, {200.0f, 100.0f}, Pink, 1.0f / 60.0f); // One fast frame
    CHECK(particles.ActiveCount() == 20);
    const Span way = DrawnSpan(particles);
    CHECK(way.low < 20.0f);
    CHECK(way.high > 195.0f);

    particles.Clear();
    emitter.Jump();
    emitter.Emit(particles, {300.0f, 100.0f}, Pink, 1.0f / 60.0f);
    CHECK(particles.ActiveCount() == 0); // It starts there: no way travelled
    emitter.Emit(particles, {310.0f, 100.0f}, Pink, 1.0f / 60.0f);
    CHECK(particles.ActiveCount() == 1);
    CHECK(DrawnSpan(particles).low > 295.0f);
}

TEST_CASE("Engine.Particles.QualityThinsAStreamEvenAtOneParticleAFrame", "[engine][fx]") {
    ParticleSystem particles({.capacity = 500});
    particles.SetQuality({.particlePercent = 50});
    ParticleEmitter emitter(Stream(60.0f, 0.0f));
    for (int frame = 0; frame < 60; ++frame)
        emitter.Emit(particles, {10.0f, 10.0f}, Pink, 1.0f / 60.0f);
    CHECK(particles.ActiveCount() == 30);

    particles.SetQuality({.particlePercent = 0});
    for (int frame = 0; frame < 60; ++frame)
        emitter.Emit(particles, {10.0f, 10.0f}, Pink, 1.0f / 60.0f);
    CHECK(particles.ActiveCount() == 30);
}

TEST_CASE("Engine.Particles.AHugeFrameKeepsTheNewestParticlesOnly", "[engine][fx]") {
    ParticleSystem particles({.capacity = 10});
    ParticleEmitter emitter(Stream(0.0f, 1.0f));
    emitter.Emit(particles, {0.0f, 100.0f}, Pink, 0.0f);
    emitter.Emit(particles, {1.0e9f, 100.0f}, Pink, 1.0f);
    CHECK(particles.ActiveCount() == 10);
    emitter.Jump();
    emitter.Emit(particles, {0.0f, 100.0f}, Pink, 0.0f);
    emitter.Emit(particles, {300.0f, 100.0f}, Pink, 1.0f);
    CHECK(DrawnSpan(particles).low > 285.0f); // The ten nearest the source
}

TEST_CASE("Engine.Particles.EmitterStylesAreChecked", "[engine][fx]") {
    CHECK(IsValid(ParticleEmitterStyle{}));                                   // Emits nothing
    CHECK(IsValid(ParticleEmitterStyle{.perSecond = 0.0f, .perUnit = 0.0f})); // Same, with any look
    CHECK_FALSE(IsValid(ParticleEmitterStyle{.perSecond = -1.0f}));
    CHECK_FALSE(IsValid(ParticleEmitterStyle{.perSecond = 1.0f})); // Emits, but its lifetime is 0
    CHECK_THROWS_AS(ParticleEmitter(ParticleEmitterStyle{.perUnit = 1.0f}), std::invalid_argument);
    CHECK(IsValid(NeonArcadeParticles.trail));
    CHECK(IsValid(NeonArcadeParticles.smoke));
    CHECK(IsValid(NeonArcadeParticles.flame));
    CHECK(IsValid(NeonArcadeParticles.embers));
}

TEST_CASE("Engine.Particles.TheStageRunsEmittersAndPassesACuesVelocity", "[engine][fx]") {
    FxStage stage(Area, Area, NeonArcadeFx);
    ParticleEmitter smoke(NeonArcadeParticles.smoke);
    for (int frame = 0; frame < 60; ++frame) {
        stage.Emit(smoke, {100.0f, 150.0f}, Pink, 1.0f / 60.0f);
        stage.UpdateReal(1.0f / 60.0f);
        stage.Update(1.0f / 60.0f);
    }
    PaintHarness h;
    h.Begin();
    stage.DrawWorldLayer(h.paint);
    h.End();
    CHECK_FALSE(h.Draws().empty());

    ParticleBurst carried{ParticleLayer{.share = 1.0f, .emission = Still}, ParticleLayer{}};
    carried[0].emission.inherit = 1.0f;
    std::vector<Float2> velocities;
    const auto record = [&](Painter&, const ParticleDrawData& data) { velocities.push_back(data.velocity); };
    stage.Clear();
    stage.Burst({{50.0f, 50.0f}, {40.0f, 0.0f}}, Pink, 3, carried, record);
    stage.SetCues(FxCueSet{{"carried", {FxParticles{carried, 2}}}});
    stage.Play("carried", {.at = {50.0f, 50.0f}, .velocity = {0.0f, 30.0f}});
    stage.UpdateReal(1.0f);
    stage.Update(1.0f);
    PaintHarness again({400.0f, 200.0f}, 1.0f);
    again.Begin();
    stage.DrawWorldLayer(again.paint);
    again.End();
    REQUIRE(velocities.size() == 3); // The cue's two draw their own shape
    CHECK(velocities[0].x == Approx(40.0f));
    REQUIRE_FALSE(again.Draws().empty());
    for (const PaintVertex& vertex : again.Drawn())
        CHECK(vertex.y > 70.0f); // Carried 30 units down in the second
}

// Not a test of behaviour: run with "[.fx-perf]" in Release to read what particles cost. Each frame emits the theme's
// soft burst (circles and sparks) so that about 'alive' particles live at once.
TEST_CASE("Engine.Particles.Cost", "[.fx-perf]") {
    const auto now = [] { return std::chrono::steady_clock::now(); };
    const auto micros = [](auto from, auto to) { return std::chrono::duration<double, std::micro>(to - from).count(); };
    for (const int alive : {600, 5000}) {
        ParticleSystem particles({.capacity = static_cast<std::size_t>(alive)});
        PaintHarness harness;
        constexpr int Warmup = 60;
        constexpr int Frames = 600;
        constexpr float Step = 1.0f / 60.0f;
        const int perFrame = std::max(1, alive / 30); // About half a second of life at 60 frames a second
        double emit = 0.0;
        double update = 0.0;
        double record = 0.0;
        double flush = 0.0;
        for (int frame = 0; frame < Warmup + Frames; ++frame) {
            const auto start = now();
            EmitBurst(particles, {200.0f, 100.0f}, Pink, perFrame, NeonArcadeParticles.soft);
            const auto emitted = now();
            particles.Update(Step);
            const auto updated = now();
            harness.Begin();
            particles.Draw(harness.paint);
            const auto recorded = now();
            harness.End();
            const auto flushed = now();
            if (frame < Warmup) continue;
            emit += micros(start, emitted);
            update += micros(emitted, updated);
            record += micros(updated, recorded);
            flush += micros(recorded, flushed);
        }
        std::printf("%zu alive: emit %.1f us, update %.1f us, draw record %.1f us, flush %.1f us per frame\n",
                    particles.ActiveCount(), emit / Frames, update / Frames, record / Frames, flush / Frames);
        CHECK(particles.ActiveCount() > static_cast<std::size_t>(alive / 2));
    }
}
