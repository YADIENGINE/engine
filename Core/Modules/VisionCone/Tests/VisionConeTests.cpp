#include "Modules/VisionCone/VisionCone.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

using Catch::Approx;

TEST_CASE("VisionCone.Geometry.ClosedSectorAndRejectionReasons", "[vision-cone]") {
    const VisionConeSpec spec{.origin = {2.0f, 3.0f}, .range = 10.0f};
    CHECK(VisionCone::Contains(spec, spec.origin));
    CHECK(VisionCone::Contains(spec, {12.0f, 3.0f})); // Range boundary
    CHECK(VisionCone::Contains(spec, {7.0f, 8.0f}));  // Positive angular boundary
    CHECK(VisionCone::Contains(spec, {7.0f, -2.0f})); // Negative angular boundary
    CHECK_FALSE(VisionCone::Contains(spec, {12.01f, 3.0f}));
    CHECK_FALSE(VisionCone::Contains(spec, {7.0f, 8.01f}));
    const auto behind = VisionCone::Query(spec, {-3.0f, 3.0f});
    CHECK(behind.status == VisionStatus::OutsideAngle);
    CHECK(behind.distance == Approx(5.0));
    CHECK_FALSE(behind.blockingFraction);
    CHECK(VisionCone::Query(spec, {-18.0f, 3.0f}).status == VisionStatus::OutOfRange);
}

TEST_CASE("VisionCone.Geometry.WrapWideSectorsAndFullCircle", "[vision-cone]") {
    VisionConeSpec spec{.direction = Pi, .range = 10.0f};
    CHECK(VisionCone::Contains(spec, {-5.0f, -1.0f})); // Across the atan2 wrap
    CHECK(VisionCone::Contains(spec, {-5.0f, 1.0f}));
    spec.direction = -4.0f * Pi;
    CHECK(VisionCone::Contains(spec, {5.0f, 0.0f}));
    CHECK_FALSE(VisionCone::Contains(spec, {-5.0f, 0.0f}));
    spec.direction = Pi / 2.0f;
    CHECK(VisionCone::Contains(spec, {0.0f, 5.0f})); // Clockwise is down
    CHECK_FALSE(VisionCone::Contains(spec, {0.0f, -5.0f}));
    spec.direction = 0.0f;
    spec.halfAngle = 3.0f * Pi / 4.0f;
    CHECK(VisionCone::Contains(spec, {-5.0f, 5.0f}));
    CHECK(VisionCone::Contains(spec, {-1.0f, -5.0f}));
    CHECK_FALSE(VisionCone::Contains(spec, {-5.0f, 0.0f}));
    spec.halfAngle = Pi;
    CHECK(VisionCone::Contains(spec, {-10.0f, 0.0f}));
    CHECK(VisionCone::Contains(spec, {0.0f, -10.0f}));
    CHECK_FALSE(VisionCone::Contains(spec, {0.0f, 10.01f}));
}

TEST_CASE("VisionCone.Query.TracesOnlyInConeAndIncludesBothSegmentEnds", "[vision-cone]") {
    const VisionConeSpec spec;
    int calls = 0;
    std::optional<float> hit;
    const VisionTrace trace = [&](Float2 from, Float2 to) -> std::optional<float> {
        CHECK(from == spec.origin);
        CHECK(VisionCone::Contains(spec, to));
        ++calls;
        return hit;
    };
    CHECK(VisionCone::Query(spec, {101.0f, 0.0f}, trace).status == VisionStatus::OutOfRange);
    CHECK(VisionCone::Query(spec, {-10.0f, 0.0f}, trace).status == VisionStatus::OutsideAngle);
    CHECK(calls == 0);
    const auto clear = VisionCone::Query(spec, {10.0f, 0.0f}, trace);
    CHECK(clear.status == VisionStatus::Visible);
    CHECK(clear.distance == Approx(10.0));
    CHECK_FALSE(clear.blockingFraction);
    for (float fraction : {0.0f, 0.5f, 1.0f}) {
        hit = fraction;
        const auto blocked = VisionCone::Query(spec, {10.0f, 0.0f}, trace);
        CHECK(blocked.status == VisionStatus::Occluded);
        REQUIRE(blocked.blockingFraction);
        CHECK(*blocked.blockingFraction == fraction);
    }
    CHECK(VisionCone::Query(spec, spec.origin, trace).status == VisionStatus::Occluded);
    CHECK(calls == 5);
    CHECK(VisionCone::Query(spec, {10.0f, 0.0f}).status == VisionStatus::Visible);
}

TEST_CASE("VisionCone.Samples.CountsExplicitPointsWithoutChoosingAIBehavior", "[vision-cone]") {
    const std::array points{Float2{10.0f, -2.0f}, Float2{10.0f, 2.0f}, Float2{-10.0f, 0.0f}, Float2{200.0f, 0.0f}};
    int calls = 0;
    const VisionTrace trace = [&](Float2, Float2 to) -> std::optional<float> {
        CHECK(to == points[static_cast<std::size_t>(calls)]);
        ++calls;
        return to.y > 0.0f ? std::optional<float>{0.5f} : std::nullopt;
    };
    const auto result = VisionCone::QuerySamples({}, points, trace);
    CHECK(result.total == 4);
    CHECK(result.inCone == 2);
    CHECK(result.visible == 1);
    CHECK(calls == 2);
    const auto empty = VisionCone::QuerySamples({}, {}, trace);
    CHECK(empty.total == 0);
    CHECK(empty.inCone == 0);
    CHECK(empty.visible == 0);
    CHECK(calls == 2);
}

TEST_CASE("VisionCone.Collision.MaskIgnoreAndLiveWorld", "[vision-cone]") {
    enum class Layer { Wall, Actor };
    CollisionWorld world;
    const Shape wall = Shape::Segment({0.0f, -5.0f}, {0.0f, 5.0f});
    const auto self = world.Add(wall, {{2.0f, 0.0f}}, MaskOf(Layer::Wall));
    const auto blocker = world.Add(wall, {{8.0f, 0.0f}}, MaskOf(Layer::Wall));
    (void)world.Add(Shape::Circle(1.0f), {{20.0f, 0.0f}}, MaskOf(Layer::Actor));
    const auto unfiltered =
        VisionCone::Query({}, {20.0f, 0.0f}, VisionCone::CollisionTrace(world, MaskOf(Layer::Wall)));
    REQUIRE(unfiltered.blockingFraction);
    CHECK(*unfiltered.blockingFraction == Approx(0.1f).margin(0.001f)); // Physics casts include contact slop
    const auto trace = VisionCone::CollisionTrace(world, MaskOf(Layer::Wall), self);
    const auto blocked = VisionCone::Query({}, {20.0f, 0.0f}, trace);
    REQUIRE(blocked.blockingFraction);
    CHECK(*blocked.blockingFraction == Approx(0.4f).margin(0.001f));
    world.Move(blocker, {{8.0f, 20.0f}});
    CHECK(VisionCone::Query({}, {20.0f, 0.0f}, trace).status == VisionStatus::Visible);
    CHECK(VisionCone::Query({}, {20.0f, 0.0f}, VisionCone::CollisionTrace(world, {})).status == VisionStatus::Visible);

    CollisionWorld atOrigin;
    const auto observer = atOrigin.Add(Shape::Circle(1.0f), {}, MaskOf(Layer::Actor));
    CHECK(VisionCone::Query({}, {}, VisionCone::CollisionTrace(atOrigin, MaskOf(Layer::Actor))).status ==
          VisionStatus::Occluded);
    CHECK(VisionCone::Query({}, {}, VisionCone::CollisionTrace(atOrigin, MaskOf(Layer::Actor), observer)).status ==
          VisionStatus::Visible);
}

TEST_CASE("VisionCone.Outline.ClockwiseSectorAndFullCircle", "[vision-cone]") {
    VisionCone cone;
    VisionConeSpec spec{.origin = {2.0f, 3.0f}, .range = 10.0f, .halfAngle = Pi / 2.0f};
    const auto sector = cone.Outline(spec, {.segments = 4});
    REQUIRE(sector.size() == 6);
    CHECK(sector.front() == spec.origin);
    CHECK(sector[1].x == Approx(2.0f).margin(1e-5f));
    CHECK(sector[1].y == Approx(-7.0f));
    CHECK(sector.back().y == Approx(13.0f));
    CHECK(sector.back() != sector.front());
    for (std::size_t i = 1; i < sector.size(); ++i) {
        CHECK(Distance(spec.origin, sector[i]) == Approx(spec.range));
        if (i > 1) CHECK(Cross(sector[i - 1] - spec.origin, sector[i] - spec.origin) > 0.0f);
    }
    spec.halfAngle = Pi;
    const auto circle = cone.Outline(spec, {.segments = 8});
    REQUIRE(circle.size() == 8);
    CHECK(circle.front() != circle.back());
    for (std::size_t i = 0; i < circle.size(); ++i) {
        CHECK(circle[i] != spec.origin);
        CHECK(Distance(spec.origin, circle[i]) == Approx(spec.range));
        CHECK(Cross(circle[i] - spec.origin, circle[(i + 1) % circle.size()] - spec.origin) > 0.0f);
    }
}

TEST_CASE("VisionCone.Outline.ClipsRaysAndReusesItsBuffer", "[vision-cone]") {
    VisionCone cone;
    int calls = 0;
    const VisionTrace wall = [&](Float2 from, Float2 to) -> std::optional<float> {
        CHECK(from == Float2{});
        ++calls;
        return to.x >= 5.0f ? std::optional<float>{5.0f / to.x} : std::nullopt;
    };
    const auto outline = cone.Outline({}, {}, wall);
    REQUIRE(outline.size() == 66);
    CHECK(calls == 65);
    for (Float2 point : outline) {
        CHECK(point.x <= 5.00001f);
        CHECK(Distance({}, point) <= 100.00001f);
    }
    const Float2* storage = outline.data();
    const auto rebuilt = cone.Outline({.origin = {10.0f, 20.0f}, .range = 2.0f});
    REQUIRE(rebuilt.size() == 66);
    CHECK(rebuilt.data() == storage);
    CHECK(rebuilt.front() == Float2{10.0f, 20.0f});
    const auto collapsed = cone.Outline({}, {}, [](Float2, Float2) { return std::optional<float>{0.0f}; });
    CHECK(collapsed.empty());
    CHECK_FALSE(cone.Outline({}).empty());
}

TEST_CASE("VisionCone.DisabledSightDoesNoTracing", "[vision-cone]") {
    const VisionConeSpec disabled{.range = 0.0f};
    int calls = 0;
    const VisionTrace trace = [&](Float2, Float2) -> std::optional<float> {
        ++calls;
        return std::nullopt;
    };
    CHECK_FALSE(VisionCone::Contains(disabled, {}));
    CHECK(VisionCone::Query(disabled, {}, trace).status == VisionStatus::OutOfRange);
    const std::array points{Float2{}, Float2{1.0f, 0.0f}};
    const auto counts = VisionCone::QuerySamples(disabled, points, trace);
    CHECK(counts.total == 2);
    CHECK(counts.inCone == 0);
    CHECK(counts.visible == 0);
    VisionCone cone;
    CHECK(cone.Outline(disabled, {}, trace).empty());
    CHECK(calls == 0);
}

TEST_CASE("VisionCone.Validation.RejectsInvalidInputsAndTraceResults", "[vision-cone]") {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const std::array invalidSpecs{VisionConeSpec{.origin = {nan, 0.0f}}, VisionConeSpec{.origin = {0.0f, inf}},
                                  VisionConeSpec{.direction = nan},      VisionConeSpec{.direction = inf},
                                  VisionConeSpec{.range = -1.0f},        VisionConeSpec{.range = inf},
                                  VisionConeSpec{.range = nan},          VisionConeSpec{.halfAngle = 0.0f},
                                  VisionConeSpec{.halfAngle = -1.0f},    VisionConeSpec{.halfAngle = Pi + 0.01f},
                                  VisionConeSpec{.halfAngle = nan},      VisionConeSpec{.halfAngle = inf}};
    VisionCone cone;
    for (const auto& spec : invalidSpecs) {
        CHECK_THROWS_AS(VisionCone::Contains(spec, {}), std::invalid_argument);
        CHECK_THROWS_AS(VisionCone::Query(spec, {}), std::invalid_argument);
        CHECK_THROWS_AS(VisionCone::QuerySamples(spec, {}), std::invalid_argument);
        CHECK_THROWS_AS(cone.Outline(spec), std::invalid_argument);
    }
    const std::array badPoints{Float2{nan, 0.0f}, Float2{0.0f, inf}};
    for (Float2 point : badPoints) {
        CHECK_THROWS_AS(VisionCone::Contains({}, point), std::invalid_argument);
        CHECK_THROWS_AS(VisionCone::Query({}, point), std::invalid_argument);
    }
    CHECK_THROWS_AS(VisionCone::QuerySamples({}, badPoints), std::invalid_argument);
    for (int segments : {-1, 0})
        CHECK_THROWS_AS(cone.Outline({}, {segments}), std::invalid_argument);
    CHECK_THROWS_AS(cone.Outline({.halfAngle = Pi}, {.segments = 2}), std::invalid_argument);
    CHECK(cone.Outline({}, {.segments = 1}).size() == 3);
    CHECK(cone.Outline({.halfAngle = Pi}, {.segments = 3}).size() == 3);
    for (float fraction : {-0.1f, 1.1f, nan, inf}) {
        const VisionTrace badTrace = [fraction](Float2, Float2) { return std::optional<float>{fraction}; };
        CHECK_THROWS_AS(VisionCone::Query({}, {1.0f, 0.0f}, badTrace), std::invalid_argument);
        CHECK_THROWS_AS(cone.Outline({}, {}, badTrace), std::invalid_argument);
    }
    const VisionTrace throwing = [](Float2, Float2) -> std::optional<float> { throw std::runtime_error("backend"); };
    CHECK_THROWS_AS(VisionCone::Query({}, {}, throwing), std::runtime_error);
    CHECK_THROWS_AS(cone.Outline({}, {}, throwing), std::runtime_error);
}
