#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>
#include "Animation/Tween.h"
#include "Diagnostics/DevelopmentChecks.h"
#include "Fx/FxStage.h"
#include "Fx/World/Bolts.h"
#include "Graphics/Atmosphere/Atmosphere.h"
#include "Graphics/Style/Theme.h"
#include "Simulation/StateHash.h"
#include "Tests/PaintHarness.h"
#include "Weather/Weather.h"

using Catch::Approx;

namespace {
constexpr CameraView ViewAt(Float2 target, float zoom = 1.0f) {
    return {.offset = {200.0f, 100.0f},
            .target = target,
            .zoom = zoom,
            .viewport = {0.0f, 0.0f, 400.0f, 200.0f}};
}

DriftLook Pieces(float density = 2.0f, float parallax = 1.0f) {
    return {.density = density, .fall = {0.0f, 100.0f}, .size = {2.0f, 2.0f}, .parallax = parallax};
}

// Every vertex drawn, across draws: nothing drawn is an empty list.
std::vector<PaintVertex> AllDrawn(const PaintHarness& h) {
    std::vector<PaintVertex> all;
    for (std::size_t draw = 0; draw < h.Draws().size(); ++draw) {
        const auto vertices = h.Drawn(draw);
        all.insert(all.end(), vertices.begin(), vertices.end());
    }
    return all;
}

// Where each drawn piece stands on the canvas, in device pixels: the middle of its quad.
std::vector<Float2> Centers(const PaintHarness& h) {
    std::vector<Float2> centers;
    const auto vertices = AllDrawn(h);
    for (std::size_t i = 0; i + 6 <= vertices.size(); i += 6) {
        Float2 sum{};
        for (std::size_t k = 0; k < 6; ++k)
            sum = sum + Float2{vertices[i + k].x, vertices[i + k].y};
        centers.push_back(sum / 6.0f);
    }
    return centers;
}

std::size_t DrawnCount(const DriftField& field, float amount, const CameraView& view) {
    PaintHarness h;
    h.Begin();
    field.Draw(h.paint, amount, view);
    h.End();
    return AllDrawn(h).size();
}

std::vector<float> FogAlphas(const FogField& fog, float amount, const CameraView& view) {
    PaintHarness h;
    h.Begin();
    fog.Draw(h.paint, amount, view);
    h.End();
    std::vector<float> alphas;
    for (const PaintVertex& vertex : AllDrawn(h))
        alphas.push_back(static_cast<float>(vertex.color.a));
    return alphas;
}

float Largest(const std::vector<float>& values) {
    return values.empty() ? 0.0f : *std::max_element(values.begin(), values.end());
}
} // namespace

TEST_CASE("Engine.Weather.IsAPoseATweenMoves", "[engine][atmosphere]") {
    STATIC_REQUIRE(AnimationPose<Weather>);
    SecondsTween<Weather> weather{Weather::Clear()};
    weather.To(Weather::Storm(), 2.0f);
    weather.Update(1.0f);
    const Weather halfway = weather.Value();
    CHECK(halfway.rain == Approx(0.5f));
    CHECK(halfway.wind.x == Approx(Weather::Storm().wind.x * 0.5f));
    weather.Update(1.0f);
    CHECK(weather.Value() == Weather::Storm());
}

TEST_CASE("Engine.Weather.ReachesASimulationsState", "[engine][atmosphere]") {
    const auto hashOf = [](const Weather& weather) {
        StateHash hash;
        VisitPose(hash, weather);
        return hash.Result();
    };
    CHECK(hashOf(Weather::Storm()) == hashOf(Weather::Storm()));
    CHECK(hashOf(Weather::Storm()) != hashOf(Weather::Rain()));
    Weather windy = Weather::Storm();
    windy.wind.y = 1.0f;
    CHECK(hashOf(windy) != hashOf(Weather::Storm()));

    // A tween of it, kept in a simulation.
    const auto tweenHash = [](float seconds) {
        SecondsTween<Weather> tween{Weather::Clear()};
        tween.To(Weather::Storm(), 4.0f);
        tween.Update(seconds);
        StateHash hash;
        tween.State(hash);
        return hash.Result();
    };
    CHECK(tweenHash(1.0f) == tweenHash(1.0f));
    CHECK(tweenHash(1.0f) != tweenHash(2.0f));
}

TEST_CASE("Engine.CameraView.ParallaxedLayersMoveByTheirShare", "[engine][atmosphere]") {
    const CameraView view{.offset = {200.0f, 100.0f}, .target = {50.0f, 20.0f}, .rotation = 0.4f, .zoom = 2.0f};
    const CameraView same = view.Parallaxed(1.0f);
    CHECK(same.target == view.target);
    CHECK(same.zoom == Approx(2.0f));
    CHECK(same.rotation == Approx(0.4f));
    const CameraView stuck = view.Parallaxed(0.0f);
    CHECK(stuck.target == Float2{});
    CHECK(stuck.zoom == Approx(1.0f));
    CHECK(stuck.rotation == Approx(0.0f));
    CHECK(view.Parallaxed(1.5f).target == Float2{75.0f, 30.0f});
}

TEST_CASE("Engine.DriftField.SameSeedSameFieldAndAmountChoosesHowMany", "[engine][atmosphere]") {
    const DriftField a(Pieces(), 3);
    const DriftField b(Pieces(), 3);
    const CameraView view = ViewAt({0.0f, 0.0f});
    const auto full = DrawnCount(a, 1.0f, view);
    CHECK(full > 0);
    CHECK(DrawnCount(b, 1.0f, view) == full);
    CHECK(DrawnCount(a, 0.0f, view) == 0);
    CHECK(DrawnCount(a, -1.0f, view) == 0);
    CHECK(DrawnCount(a, 0.5f, view) < full);
    CHECK(DrawnCount(a, 0.5f, view) > 0);
    CHECK(DrawnCount(a, 3.0f, view) == full);
}

TEST_CASE("Engine.DriftField.FillsTheViewWhereverTheCameraGoes", "[engine][atmosphere]") {
    const DriftField field(Pieces(4.0f));
    const auto home = DrawnCount(field, 1.0f, ViewAt({0.0f, 0.0f}));
    REQUIRE(home > 0);
    for (const Float2 target : {Float2{5000.0f, 0.0f}, Float2{-123456.0f, 98765.0f}, Float2{1536.0f, 1024.0f}}) {
        const auto there = DrawnCount(field, 1.0f, ViewAt(target));
        CHECK(there > home / 2);
        CHECK(there < home * 2);
    }
    // Zoomed out it shows more of the same world, so more pieces, up to one tile's worth, however far it goes.
    const auto wide = DrawnCount(field, 1.0f, ViewAt({0.0f, 0.0f}, 0.25f));
    CHECK(wide > home * 4);
    const auto ceiling = (field.Capacity() + 1U) * 6U * 2U; // Six vertices each; a few repeat across the view's edges
    CHECK(DrawnCount(field, 1.0f, ViewAt({0.0f, 0.0f}, 0.02f)) < ceiling);
}

TEST_CASE("Engine.DriftField.ParallaxDecidesHowItFollowsTheCamera", "[engine][atmosphere]") {
    const auto centers = [](const DriftField& field, Float2 target) {
        PaintHarness h;
        h.Begin();
        field.Draw(h.paint, 1.0f, ViewAt(target));
        h.End();
        return Centers(h);
    };
    const DriftField stuck(Pieces(2.0f, 0.0f));
    const auto a = centers(stuck, {0.0f, 0.0f});
    const auto b = centers(stuck, {300.0f, 90.0f});
    REQUIRE(a.size() == b.size());
    REQUIRE_FALSE(a.empty());
    for (std::size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].x == Approx(b[i].x));
        CHECK(a[i].y == Approx(b[i].y));
    }
    // In the world: moving the camera by a whole tile shows the same picture, and by anything less a moved one.
    const DriftField world(Pieces(2.0f, 1.0f));
    const auto c = centers(world, {0.0f, 0.0f});
    const auto d = centers(world, {1536.0f, 1024.0f});
    const auto e = centers(world, {100.0f, 0.0f});
    REQUIRE(c.size() == d.size());
    for (std::size_t i = 0; i < c.size(); ++i)
        CHECK(c[i].x == Approx(d[i].x).margin(0.01));
    CHECK((c.size() != e.size() || c.front().x != Approx(e.front().x)));
}

TEST_CASE("Engine.DriftField.UpdateMovesByFallAndWindAndKeepsToItsTile", "[engine][atmosphere]") {
    DriftField field({.density = 0.01f, .fall = {0.0f, 100.0f}, .windShare = 0.5f, .size = {2.0f, 2.0f}});
    const auto center = [&](float width, float height) {
        PaintHarness h({width, height});
        h.Begin();
        field.Draw(h.paint, 1.0f, {.viewport = {0.0f, 0.0f, width, height}});
        h.End();
        return Centers(h);
    };
    const auto before = center(1536.0f, 1024.0f);
    REQUIRE_FALSE(before.empty());
    field.Update(0.5f, {40.0f, 0.0f});
    const auto after = center(1536.0f, 1024.0f);
    REQUIRE(after.size() == before.size());
    CHECK(after[0].x - before[0].x == Approx(10.0f));
    field.Update(std::numeric_limits<float>::quiet_NaN(), {});
    field.Update(-1.0f, {});
    CHECK(center(1536.0f, 1024.0f)[0].x == Approx(after[0].x));
    field.Update(100000.0f, {1e6f, -1e6f});
    CHECK_FALSE(center(1536.0f, 1024.0f).empty());
}

TEST_CASE("Engine.DriftField.OwnDrawingAndInvalidLooks", "[engine][atmosphere]") {
    int drawn = 0;
    DriftLook own = Pieces();
    own.draw = [&drawn](Painter&, const ParticleDrawData&) { ++drawn; };
    const DriftField field(own);
    PaintHarness h;
    h.Begin();
    field.Draw(h.paint, 1.0f, ViewAt({0.0f, 0.0f}));
    h.End();
    CHECK(drawn > 0);
    CHECK(AllDrawn(h).empty());

    CHECK_THROWS_AS(DriftField({.density = -1.0f}), std::invalid_argument);
    CHECK_THROWS_AS(DriftField({.twinkle = 2.0f}), std::invalid_argument);
    CHECK_THROWS_AS(DriftField({.pace = {2.0f, 1.0f}}), std::invalid_argument);
    CHECK_THROWS_AS(FogField({.wavelength = 0.0f}), std::invalid_argument);
    CHECK_THROWS_AS(FogField({.band = FogBand{.top = 10.0f, .bottom = 0.0f}}), std::invalid_argument);
    CHECK_THROWS_AS(GroundRipples({.seconds = 0.0f}), std::invalid_argument);
}

TEST_CASE("Engine.Atmosphere.PartsRefuseAnAppliedCamera", "[engine][atmosphere]") {
    PaintHarness h;
    const FogField fog(FogLook{});
    const DriftField drift(Pieces());
    const GroundRipples ripples({.density = 1.0f});
    h.Begin();
    {
        auto camera = h.paint.Camera(ViewAt({10.0f, 0.0f}));
        if (DevelopmentChecks()) {
            CHECK_THROWS_AS(fog.Draw(h.paint, 1.0f), std::invalid_argument);
            CHECK_THROWS_AS(drift.Draw(h.paint, 1.0f), std::invalid_argument);
            CHECK_THROWS_AS(ripples.Draw(h.paint, 1.0f), std::invalid_argument);
        } else {
            fog.Draw(h.paint, 1.0f);
            drift.Draw(h.paint, 1.0f);
            ripples.Draw(h.paint, 1.0f);
            CHECK(h.list.Rejected() == 3);
        }
    }
    h.End();
}

TEST_CASE("Engine.FogField.AmountThickensItAndBandBoundsIt", "[engine][atmosphere]") {
    const FogField fog({.color = {255, 255, 255, 200}, .wavelength = 200.0f});
    const CameraView view = ViewAt({0.0f, 0.0f});
    CHECK(FogAlphas(fog, 0.0f, view).empty());
    const auto thin = FogAlphas(fog, 0.2f, view);
    const auto thick = FogAlphas(fog, 1.0f, view);
    REQUIRE(thin.size() == thick.size());
    float thinSum = 0.0f;
    float thickSum = 0.0f;
    for (std::size_t i = 0; i < thin.size(); ++i) {
        CHECK(thin[i] <= thick[i]);
        thinSum += thin[i];
        thickSum += thick[i];
    }
    CHECK(thinSum < thickSum);
    CHECK(Largest(thick) <= 200.0f);

    // A band far from the view leaves nothing; one over it leaves fog.
    const FogField away(
        {.color = {255, 255, 255, 200}, .band = FogBand{.top = 5000.0f, .bottom = 5100.0f, .soft = 10.0f}});
    CHECK(Largest(FogAlphas(away, 1.0f, view)) == 0.0f);
    const FogField over(
        {.color = {255, 255, 255, 200}, .band = FogBand{.top = -500.0f, .bottom = 500.0f, .soft = 10.0f}});
    CHECK(Largest(FogAlphas(over, 1.0f, view)) > 0.0f);
}

TEST_CASE("Engine.FogField.StandsInTheWorldAndMovesWithTheWind", "[engine][atmosphere]") {
    FogField fog({.color = {255, 255, 255, 255}, .wavelength = 300.0f, .boil = 0.0f});
    const CameraView view = ViewAt({0.0f, 0.0f});
    const auto before = FogAlphas(fog, 0.8f, view);
    CHECK(FogAlphas(fog, 0.8f, view) == before);
    fog.Update(1.0f, {100.0f, 0.0f});
    CHECK(FogAlphas(fog, 0.8f, view) != before);
    CHECK(FogAlphas(fog, 0.8f, ViewAt({-60.0f, 0.0f})) != before);
}

TEST_CASE("Engine.Atmosphere.ShowsOnlyWhatTheWeatherAsksFor", "[engine][atmosphere]") {
    const auto drawn = [](const Weather& weather, bool front) {
        Atmosphere air;
        air.Update(0.016f, weather);
        PaintHarness h;
        h.Begin();
        if (front)
            air.DrawFront(h.paint, ViewAt({0.0f, 0.0f}));
        else
            air.DrawBehind(h.paint, ViewAt({0.0f, 0.0f}));
        h.End();
        return AllDrawn(h).size();
    };
    CHECK(drawn(Weather::Clear(), false) == 0);
    CHECK(drawn(Weather::Clear(), true) == 0);
    CHECK(drawn({.rain = 1.0f}, false) > 0);
    CHECK(drawn({.rain = 1.0f}, true) > 0);
    CHECK(drawn({.snow = 1.0f}, true) > 0);
    CHECK(drawn({.dust = 1.0f}, true) > 0);
    CHECK(drawn({.fog = 1.0f}, false) > 0);
    CHECK(drawn({.rain = std::numeric_limits<float>::quiet_NaN()}, true) == 0);
}

TEST_CASE("Engine.Atmosphere.SideViewHasNoRipplesAndFogAlongTheGround", "[engine][atmosphere]") {
    const AtmosphereLook top = TopDownAtmosphere();
    const AtmosphereLook side = SideViewAtmosphere(600.0f);
    CHECK(top.ripples.density > 0.0f);
    CHECK_FALSE(top.fogBehind.band);
    CHECK(side.ripples.density == 0.0f);
    REQUIRE(side.fogBehind.band);
    CHECK(side.fogBehind.band->bottom > 600.0f);
    CHECK(side.fogBehind.band->top < 600.0f);
}

TEST_CASE("Engine.Bolts.StrikeThePointAndRespectFlashes", "[engine][atmosphere][fx]") {
    FlashLimit limit;
    Bolts bolts;
    const FloatRect area{0.0f, 0.0f, 400.0f, 200.0f};
    bolts.Strike(limit, {100.0f, 150.0f}, area, {255, 255, 255, 255}, {});
    CHECK(bolts.ActiveCount() == 1);
    // The limit shares with other flashes: another start at once does not happen.
    bolts.Strike(limit, {100.0f, 150.0f}, area, {255, 255, 255, 255}, {});
    CHECK(bolts.ActiveCount() == 1);

    PaintHarness h;
    h.Begin();
    bolts.Draw(h.paint);
    h.End();
    CHECK_FALSE(AllDrawn(h).empty());

    bolts.Update(0.3f);
    CHECK(bolts.ActiveCount() == 0);

    bolts.SetQuality({.flashes = false});
    limit.Clear();
    bolts.Strike(limit, {100.0f, 150.0f}, area, {255, 255, 255, 255}, {});
    CHECK(bolts.ActiveCount() == 0);
    CHECK_THROWS_AS(bolts.Strike(limit, {}, area, {}, {.seconds = 0.0f}), std::invalid_argument);
    CHECK_THROWS_AS((FxCue{FxBolt{.width = -1.0f}}), std::invalid_argument);
}

TEST_CASE("Engine.FxStage.TheLightningCueLightsTheScene", "[engine][atmosphere][fx]") {
    const FloatRect area{0.0f, 0.0f, 400.0f, 200.0f};
    FxStage stage(area, area, CurrentTheme().fx);
    stage.Play("lightning", {.at = {100.0f, 150.0f}});
    const LightFlashFrame frame = stage.LightFlashes();
    CHECK(frame.ambient.r > 0);
    PaintHarness h;
    h.Begin();
    stage.DrawWorldLayer(h.paint);
    h.End();
    CHECK_FALSE(AllDrawn(h).empty());
}
