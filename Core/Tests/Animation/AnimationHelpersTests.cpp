#include <catch2/catch_approx.hpp>
#include "Tests/Support/Diagnostics.h"
#include <stdexcept>
#include "Animation/AnimatedCounter.h"
#include "Animation/ButtonAnim.h"
#include "Animation/SpringPoint.h"

TEST_CASE("Engine.ButtonAnim.DefaultsAndNonpositiveTime", "[engine][graphics]") {
    ButtonAnim button;
    button.Update(true, 0.0f);
    button.Update(true, -1.0f);
    CHECK(button.State().highlight == 0.0f);
    button.Update(true, 0.1f);
    CHECK(button.State().highlight == 1.0f);
    button.Press();
    button.Deny();
    CHECK(button.State().press == 0.0f);
    CHECK(button.State().shake == 0.0f);
    button.Update(false, 0.1f);
    CHECK(button.State().highlight == 0.0f);
}

TEST_CASE("Engine.ButtonAnim.ConfigurableResponseAndFeedback", "[engine][graphics]") {
    ButtonAnim fast({.highlightStiffness = 100.0f,
                     .highlightDamping = 10.0f,
                     .maxStep = 0.05f,
                     .pressSeconds = 0.4f,
                     .denySeconds = 0.8f});
    ButtonAnim slow({.highlightStiffness = 25.0f, .highlightDamping = 10.0f, .maxStep = 0.05f});
    fast.Update(true, 0.1f);
    slow.Update(true, 0.1f);
    CHECK(fast.State().highlight > slow.State().highlight);
    CHECK(fast.State().highlight < 1.0f);
    fast.Press();
    fast.Deny();
    CHECK(fast.State().press > 0.0f);
    CHECK(fast.State().shake > 0.0f);
    fast.Update(true, 0.2f);
    CHECK(fast.State().press > 0.0f);
    CHECK(fast.State().press < 1.0f);
    CHECK(fast.State().shake > 0.0f);
    CHECK(fast.State().shake < 1.0f);
    fast.Update(false, 1.0f);
    CHECK(fast.State().press == 0.0f);
    CHECK(fast.State().shake == 0.0f);
}

TEST_CASE("Engine.ButtonAnim.ClearRetainsSettings", "[engine][graphics]") {
    const ButtonAnimSettings settings{
        .highlightStiffness = 320.0f, .highlightDamping = 16.0f, .pressSeconds = 0.3f, .denySeconds = 0.35f};
    ButtonAnim button(settings);
    ButtonAnim fresh(settings);
    button.Press();
    button.Deny();
    button.Update(true, 0.1f);
    button.Clear();
    CHECK(button.State().highlight == 0.0f);
    CHECK(button.State().press == 0.0f);
    CHECK(button.State().shake == 0.0f);
    button.Press();
    fresh.Press();
    button.Deny();
    fresh.Deny();
    button.Update(true, 0.05f);
    fresh.Update(true, 0.05f);
    CHECK(button.State().highlight == Catch::Approx(fresh.State().highlight));
    CHECK(button.State().press == Catch::Approx(fresh.State().press));
    CHECK(button.State().shake == Catch::Approx(fresh.State().shake));
    CHECK_THROWS_AS(ButtonAnim(ButtonAnimSettings{.pressSeconds = -1.0f}), std::invalid_argument);
    CHECK_THROWS_AS(ButtonAnim(ButtonAnimSettings{.maxStep = 0.0f}), std::invalid_argument);
}
TEST_CASE("Engine.AnimatedCounter.IncreasesDecreasesAndClears", "[engine][animation]") {
    AnimatedCounter counter({.speed = 8.0f, .punchSeconds = 0.3f});
    counter.Update(100, 0.0625f);
    CHECK(counter.Shown() == 50.0);
    CHECK(counter.Punch() > 0.0f);
    counter.Update(20, 0.0625f);
    CHECK(counter.Shown() == 20.0);
    counter.Clear();
    CHECK(counter.Shown() == 0.0);
    CHECK(counter.Target() == 0);
    CHECK(counter.Punch() == 0.0f);
}
TEST_CASE("Engine.SpringPoint.FollowsAndClampsLongFrames", "[engine][animation]") {
    SpringPoint point;
    point.Snap({10.0f, 20.0f});
    CHECK(point.position.x == 10.0f);
    CHECK(point.velocity.y == 0.0f);
    SpringPoint clamped = point;
    SpringPoint stalled = point;
    clamped.Follow({100.0f, 20.0f}, 0.1f, 40.0f, 8.0f, 0.1f);
    stalled.Follow({100.0f, 20.0f}, 5.0f, 40.0f, 8.0f, 0.1f); // A stall moves it no further than the longest step
    CHECK(clamped.position.x > 10.0f);
    CHECK(stalled.position.x == clamped.position.x);
}
