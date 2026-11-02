#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <stdexcept>
#include <vector>
#include "Modules/Popup/Popup.h"
#include "Tests/PaintHarness.h"

namespace {
PopupStyle Still() {
    PopupStyle style;
    style.enter = {};
    style.holdSeconds = 1.0f;
    style.exit = {};
    style.motion = {};
    return style;
}

void Content(Painter&, const PopupFrame&) {}
} // namespace

TEST_CASE("Popup.DelaysExpiresAndIgnoresInvalidDelta", "[popup]") {
    Popup popups;
    auto style = Still();
    style.delaySeconds = 0.25f;
    const auto handle = popups.Emit({}, Content, style);
    REQUIRE(popups.Frame(handle));
    CHECK(popups.Frame(handle)->phase == PopupPhase::Delayed);
    popups.Update(-1.0f);
    popups.Update(std::numeric_limits<float>::quiet_NaN());
    CHECK(popups.Frame(handle)->phase == PopupPhase::Delayed);
    popups.Update(0.25f);
    CHECK(popups.Frame(handle)->phase == PopupPhase::Hold);
    popups.Update(1.0f);
    CHECK_FALSE(popups.IsAlive(handle));
    CHECK_FALSE(popups.Frame(handle));
    CHECK(popups.ActiveCount() == 0);
}

TEST_CASE("Popup.HandlesCannotCrossOwnersOrBeReusedAfterClear", "[popup]") {
    Popup first;
    Popup second;
    const auto old = first.Emit({}, Content);
    const auto other = second.Emit({}, Content);
    CHECK_FALSE(first.IsAlive(other));
    CHECK_FALSE(second.Cancel(old));
    first.Clear();
    const auto fresh = first.Emit({}, Content);
    CHECK(old != fresh);
    CHECK_FALSE(first.SetAnchor(old, {}));
    CHECK(first.Cancel(fresh));
    CHECK_FALSE(first.Cancel(fresh));
}

TEST_CASE("Popup.CapacityEvictsByAgeAndDrawsByLayer", "[popup]") {
    Popup popups({.capacity = 2});
    std::vector<int> order;
    auto top = Still();
    top.layer = 10;
    const auto oldest = popups.Emit({}, [&](Painter&, const PopupFrame&) { order.push_back(1); }, top);
    const auto second = popups.Emit({}, [&](Painter&, const PopupFrame&) { order.push_back(2); }, Still());
    PaintFrame([&](Painter& paint) { popups.Draw(paint); });
    CHECK(order == std::vector<int>{2, 1});
    const auto third = popups.Emit({}, Content, Still());
    CHECK_FALSE(popups.IsAlive(oldest));
    CHECK(popups.IsAlive(second));
    CHECK(popups.IsAlive(third));
    Popup rejecting({.capacity = 1, .overflow = PopupOverflow::RejectNew});
    CHECK(static_cast<bool>(rejecting.Emit({}, Content)));
    CHECK_FALSE(static_cast<bool>(rejecting.Emit({}, Content)));
    Popup disabled({.capacity = 0});
    CHECK_FALSE(static_cast<bool>(disabled.Emit({}, Content)));
}

TEST_CASE("Popup.AnchorAndEarlyDismissPreserveTheCurrentPose", "[popup]") {
    Popup popups;
    const auto handle = popups.Emit({30.0f, 40.0f}, Content, Popup::Preset(PopupPreset::Bounce));
    popups.Update(0.0625f);
    const auto before = *popups.Frame(handle);
    CHECK(popups.SetAnchor(handle, {50.0f, 60.0f}));
    const auto moved = *popups.Frame(handle);
    CHECK(moved.ageSeconds == before.ageSeconds);
    CHECK(moved.position.x == Catch::Approx(before.position.x + 20.0f));
    CHECK(moved.position.y == Catch::Approx(before.position.y + 20.0f));
    CHECK(popups.Dismiss(handle));
    const auto dismissed = *popups.Frame(handle);
    CHECK(dismissed.phase == PopupPhase::Exit);
    CHECK(dismissed.position == moved.position);
    CHECK(dismissed.scale == moved.scale);
    CHECK(dismissed.opacity == moved.opacity);
    popups.Update(1.0f);
    CHECK_FALSE(popups.IsAlive(handle));
}

TEST_CASE("Popup.ExtensionIsBoundedAndExitCannotRestart", "[popup]") {
    Popup popups;
    auto style = Still();
    style.holdSeconds = 0.75f;
    style.exit = {.seconds = 0.25f, .opacity = 0.0f};
    const auto handle = popups.Emit({}, Content, style);
    for (int i = 0; i < 8; ++i)
        CHECK(popups.Extend(handle, 0.5f, 2.0f));
    popups.Update(1.875f);
    CHECK(popups.Frame(handle)->phase == PopupPhase::Exit);
    CHECK_FALSE(popups.Extend(handle, 1.0f, 4.0f));
    CHECK(popups.Dismiss(handle));
    popups.Update(0.125f);
    CHECK_FALSE(popups.IsAlive(handle));
}

TEST_CASE("Popup.DrawScopesRecoverAfterCallbackFailureAndRejectMutation", "[popup]") {
    Popup popups;
    popups.Emit(
        {},
        [&](Painter&, const PopupFrame&) {
            CHECK_THROWS_AS(popups.Clear(), std::logic_error);
            throw std::runtime_error("drawing failed");
        },
        Still());
    PaintHarness harness;
    harness.Begin();
    CHECK_THROWS_AS(popups.Draw(harness.paint), std::runtime_error);
    popups.Clear();
    CHECK_NOTHROW(harness.End());
}

TEST_CASE("Popup.RejectsInvalidInputsBeforeChangingTheCollection", "[popup]") {
    Popup popups;
    auto style = Still();
    style.enter.easing = nullptr;
    CHECK_THROWS_AS(popups.Emit({}, Content, style), std::invalid_argument);
    CHECK_THROWS_AS(popups.Emit({}, {}), std::invalid_argument);
    style = Still();
    style.holdSeconds = 0.0f;
    CHECK_THROWS_AS(popups.Emit({}, Content, style), std::invalid_argument);
    CHECK(popups.ActiveCount() == 0);
}
