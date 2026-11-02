#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>
#include "Graphics/Text/FontFace.h"
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

TEST_CASE("Popup.PulseOverlaysAndDecaysWithoutAccumulating", "[popup]") {
    Popup popups;
    const auto handle = popups.Emit({}, Content, Still());
    CHECK(popups.Pulse(handle, {.seconds = 0.5f, .scale = {1.0f, -0.5f}}));
    CHECK(popups.Pulse(handle, {.seconds = 0.5f, .scale = {1.0f, -0.5f}}));
    const auto pulsed = *popups.Frame(handle);
    CHECK(pulsed.scale.x == Catch::Approx(2.0f));
    CHECK(pulsed.scale.y == Catch::Approx(0.5f));
    popups.Update(0.5f);
    CHECK(popups.Frame(handle)->scale.x == Catch::Approx(1.0f));
    CHECK(popups.Frame(handle)->scale.y == Catch::Approx(1.0f));
}

TEST_CASE("Popup.DragSlowsTheFlightIndependentlyOfUpdateSteps", "[popup]") {
    auto style = Still();
    style.motion = {.velocity = {0.0f, -100.0f}, .drag = 4.0f};
    Popup whole;
    Popup stepped;
    const auto a = whole.Emit({}, Content, style);
    const auto b = stepped.Emit({}, Content, style);
    whole.Update(0.5f);
    for (int i = 0; i < 4; ++i)
        stepped.Update(0.125f);
    const float early = whole.Frame(a)->position.y;
    CHECK(early == Catch::Approx(stepped.Frame(b)->position.y));
    CHECK(early < 0.0f);
    CHECK(early > style.motion.velocity.y * 0.5f);
    whole.Update(0.25f);
    const float travelled = whole.Frame(a)->position.y - early;
    CHECK(travelled < 0.0f);
    CHECK(travelled > early * 0.5f);
}

TEST_CASE("Popup.TextLabelPivotsAtItsMiddle", "[popup]") {
    const auto font = FontFace::FromFile(ENGINE_TEST_FONT);
    constexpr float Size = 24.0f;
    constexpr Float2 Origin{200.0f, 100.0f};
    Popup popups;
    popups.Emit(Origin, PopupText("BONUS", {.size = Size}), Still());
    PaintHarness harness({400.0f, 200.0f}, 1.0f);
    harness.context.font = &font;
    harness.Begin();
    popups.Draw(harness.paint);
    harness.End();
    REQUIRE_FALSE(harness.Draws().empty());
    float left = std::numeric_limits<float>::max();
    float right = std::numeric_limits<float>::lowest();
    float top = left;
    float bottom = right;
    for (const auto& vertex : harness.Drawn()) {
        left = std::min(left, vertex.x);
        right = std::max(right, vertex.x);
        top = std::min(top, vertex.y);
        bottom = std::max(bottom, vertex.y);
    }
    CHECK((left + right) * 0.5f == Catch::Approx(Origin.x).margin(Size * 0.25f));
    CHECK((top + bottom) * 0.5f == Catch::Approx(Origin.y).margin(Size * 0.25f));
}

TEST_CASE("Popup.StrictDefaultFollowsDevelopmentChecks", "[popup]") {
    const bool original = DevelopmentChecks();
    SetDevelopmentChecks(false);
    const PopupSettings relaxed;
    SetDevelopmentChecks(true);
    const PopupSettings strict;
    SetDevelopmentChecks(original);
    CHECK_FALSE(relaxed.strict);
    CHECK(strict.strict);
}

TEST_CASE("Popup.NonStrictSkipsAndCountsInvalidInput", "[popup]") {
    Popup popups({.strict = false});
    auto style = Still();
    style.enter.easing = nullptr;
    CHECK_FALSE(static_cast<bool>(popups.Emit({}, Content, style)));
    CHECK_FALSE(static_cast<bool>(popups.Emit({}, {})));
    CHECK(popups.ActiveCount() == 0);
    CHECK(popups.RejectedCount() == 2);
    CHECK_FALSE(popups.FirstRejection().empty());
    auto broken = Still();
    broken.enter = {.seconds = 1.0f, .easing = [](float) { return std::numeric_limits<float>::quiet_NaN(); }};
    const auto handle = popups.Emit({}, Content, broken);
    popups.Update(0.5f);
    REQUIRE(popups.Frame(handle));
    CHECK(popups.Frame(handle)->opacity == Catch::Approx(1.0f));
    CHECK(popups.RejectedCount() > 2);
}

TEST_CASE("Popup.PresetDefaultsToTheDefaultStyleAndDurationSumsPhases", "[popup]") {
    const auto preset = Popup::Preset({});
    const PopupStyle plain;
    CHECK(preset.holdSeconds == plain.holdSeconds);
    CHECK(preset.enter.seconds == plain.enter.seconds);
    auto style = Still();
    style.enter.seconds = 0.25f;
    style.exit.seconds = 0.5f;
    CHECK(Popup::Duration(style) == Catch::Approx(1.75f));
    Popup popups;
    CHECK(popups.IsAlive(popups.Emit({}, Content, PopupPreset::Float)));
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
