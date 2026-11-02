#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <stdexcept>
#include "Modules/NumberPopup/NumberPopup.h"

namespace {
NumberPopupStyle Still() {
    NumberPopupStyle style;
    style.popup.enter = {};
    style.popup.holdSeconds = 0.75f;
    style.popup.exit = {.seconds = 0.25f, .opacity = 0.0f};
    style.popup.motion = {};
    style.juice.scatterVelocity = {};
    style.juice.rotationSpread = 0.0f;
    style.merge.windowSeconds = 0.25f;
    style.merge.extendSeconds = 0.25f;
    style.merge.maxLifetimeSeconds = 1.5f;
    return style;
}

constexpr NumberPopupEmit Keyed(std::uint64_t key) { return {.mergeKey = {key}}; }

enum class TestGroup { First, Second };
} // namespace

TEST_CASE("NumberPopup.FormatsExactIntegersAndSignedDecimals", "[number-popup]") {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto minimum = std::numeric_limits<std::int64_t>::min();
    CHECK(NumberPopup::Format(maximum) == "9223372036854775807");
    CHECK(NumberPopup::Format(minimum) == "-9223372036854775808");
    NumberPopupFormat format;
    format.sign = NumberPopupSign::Always;
    format.decimals = 2;
    format.suffix = " HP";
    CHECK(NumberPopup::Format(12, format) == "+12.00 HP");
    CHECK(NumberPopup::Format(-2.5, format) == "-2.50 HP");
    CHECK(NumberPopup::Format(-0.001, format) == "+0.00 HP");
    format.trimTrailingZeros = true;
    CHECK(NumberPopup::Format(2.5f, format) == "+2.5 HP");
    format.sign = NumberPopupSign::None;
    CHECK(NumberPopup::Format(-2.5, format) == "2.5 HP");
}

TEST_CASE("NumberPopup.CompactRoundingPromotesTheUnit", "[number-popup]") {
    NumberPopupFormat format;
    format.decimals = 1;
    format.compact = true;
    CHECK(NumberPopup::Format(1500, format) == "1.5K");
    CHECK(NumberPopup::Format(999999, format) == "1.0M");
    CHECK(NumberPopup::Format(-1260000, format) == "-1.3M");
}

TEST_CASE("NumberPopup.AddPreservesIntegerPrecisionAndSaturates", "[number-popup]") {
    NumberPopup numbers;
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto handle = numbers.Emit({}, maximum - 2, Still());
    CHECK(numbers.Add(handle, 2));
    REQUIRE(numbers.Value(handle));
    CHECK(std::get<std::int64_t>(*numbers.Value(handle)) == maximum);
    CHECK(numbers.Add(handle, 1));
    CHECK(std::get<std::int64_t>(*numbers.Value(handle)) == maximum);
    const auto decimal = numbers.Emit({}, std::numeric_limits<double>::max(), Still());
    CHECK(numbers.Add(decimal, std::numeric_limits<double>::max()));
    CHECK(std::get<double>(*numbers.Value(decimal)) == std::numeric_limits<double>::max());
    numbers.Clear();
    CHECK_FALSE(numbers.Value(handle));
    CHECK_FALSE(numbers.Add(handle, 1));
}

TEST_CASE("NumberPopup.MergesOnlyCompatibleKeysWithinTheWindow", "[number-popup]") {
    NumberPopup numbers;
    auto style = Still();
    const auto first = numbers.Emit({20.0f, 30.0f}, 120, style, Keyed(42));
    numbers.Update(0.125f);
    const auto before = *numbers.Frame(first);
    const auto merged = numbers.Emit({200.0f, 300.0f}, 30, style, Keyed(42));
    CHECK(merged == first);
    CHECK(std::get<std::int64_t>(*numbers.Value(first)) == 150);
    CHECK(numbers.Frame(first)->position == before.position);
    CHECK(numbers.Frame(first)->ageSeconds == before.ageSeconds);
    CHECK(numbers.Emit({}, -10, style, Keyed(42)) != first);
    CHECK(numbers.Emit({}, 10, style, Keyed(43)) != first);
    CHECK(numbers.Emit({}, 10, style) != first);
    style.merge.windowSeconds = 0.0f;
    CHECK(numbers.Emit({}, 10, style, Keyed(42)) != first);
    numbers.Update(0.5f);
    CHECK(numbers.Emit({}, 10, Still(), Keyed(42)) != first);
}

TEST_CASE("NumberPopup.FollowAnchorMovesAMergedEntry", "[number-popup]") {
    NumberPopup numbers;
    auto style = Still();
    style.merge.followAnchor = true;
    const auto first = numbers.Emit({20.0f, 30.0f}, 1, style, Keyed(1));
    CHECK(numbers.Emit({200.0f, 300.0f}, 1, style, Keyed(1)) == first);
    CHECK(numbers.Frame(first)->position == Float2{200.0f, 300.0f});
}

TEST_CASE("NumberPopup.MergeWindowStartsWhenADelayedEntryActivates", "[number-popup]") {
    NumberPopup numbers;
    auto style = Still();
    style.popup.delaySeconds = 1.0f;
    const auto first = numbers.Emit({}, 1, style, Keyed(1));
    numbers.Update(0.75f);
    CHECK(numbers.Emit({}, 1, style, Keyed(1)) == first);
    numbers.Update(0.375f);
    CHECK(numbers.Frame(first)->phase != PopupPhase::Delayed);
    CHECK(numbers.Emit({}, 1, style, Keyed(1)) == first);
    numbers.Update(0.5f);
    CHECK(numbers.Emit({}, 1, style, Keyed(1)) != first);
}

TEST_CASE("NumberPopup.MergeKeyOfIsNonzeroAndDistinguishesParts", "[number-popup]") {
    STATIC_REQUIRE(NumberPopupMergeKey::Of(TestGroup::First, 1).value != 0);
    STATIC_REQUIRE(NumberPopupMergeKey::Of(TestGroup::First, 1) == NumberPopupMergeKey::Of(TestGroup::First, 1));
    STATIC_REQUIRE(NumberPopupMergeKey::Of(TestGroup::First, 1) != NumberPopupMergeKey::Of(TestGroup::Second, 1));
    STATIC_REQUIRE(NumberPopupMergeKey::Of(TestGroup::First, 1) != NumberPopupMergeKey::Of(TestGroup::First, 2));
}

TEST_CASE("NumberPopup.RepeatedAccumulationCannotKeepAnEntryAliveForever", "[number-popup]") {
    NumberPopup numbers;
    const auto handle = numbers.Emit({}, 1, Still());
    for (int i = 0; i < 8; ++i)
        CHECK(numbers.Add(handle, 1));
    numbers.Update(1.375f);
    REQUIRE(numbers.Frame(handle));
    CHECK(numbers.Frame(handle)->phase == PopupPhase::Exit);
    CHECK_FALSE(numbers.Add(handle, 1));
    CHECK_FALSE(numbers.Pulse(handle));
    numbers.Update(0.125f);
    CHECK_FALSE(numbers.IsAlive(handle));
}

TEST_CASE("NumberPopup.AddExtendsHoldWithoutAutoMerge", "[number-popup]") {
    NumberPopup numbers;
    auto style = Still();
    style.merge.windowSeconds = 0.0f;
    const auto handle = numbers.Emit({}, 1, style);
    CHECK(numbers.Add(handle, 1));
    numbers.Update(0.875f);
    CHECK(numbers.Frame(handle)->phase == PopupPhase::Hold);
}

TEST_CASE("NumberPopup.FormattingFailureLeavesTheVisibleValueUnchanged", "[number-popup]") {
    NumberPopup numbers;
    auto style = Still();
    style.format.formatter = [](const NumberPopupValue& value) {
        if (std::get<std::int64_t>(value) > 10) throw std::runtime_error("formatter failed");
        return std::string("label");
    };
    const auto handle = numbers.Emit({}, 5, style);
    CHECK_THROWS_AS(numbers.Add(handle, 10), std::runtime_error);
    CHECK(std::get<std::int64_t>(*numbers.Value(handle)) == 5);
}

TEST_CASE("NumberPopup.OwnersCapacityAndClearInvalidateHandles", "[number-popup]") {
    NumberPopup numbers({.capacity = 1});
    NumberPopup other;
    const auto first = numbers.Emit({}, 1);
    CHECK_FALSE(other.Add(first, 1));
    const auto replacement = numbers.Emit({}, 2);
    CHECK_FALSE(numbers.IsAlive(first));
    CHECK_FALSE(numbers.Value(first));
    CHECK_FALSE(numbers.Add(first, 1));
    CHECK(numbers.IsAlive(replacement));
    numbers.Clear();
    const auto fresh = numbers.Emit({}, 3);
    CHECK(fresh != replacement);
    CHECK_FALSE(numbers.Cancel(replacement));
    CHECK(numbers.Cancel(fresh));
    NumberPopup rejecting({.capacity = 1, .overflow = PopupOverflow::RejectNew});
    CHECK(static_cast<bool>(rejecting.Emit({}, 1)));
    CHECK_FALSE(static_cast<bool>(rejecting.Emit({}, 2)));
}

TEST_CASE("NumberPopup.DismissExitsAndCancelRemoves", "[number-popup]") {
    NumberPopup numbers;
    const auto dismissed = numbers.Emit({}, 1, Still());
    const auto cancelled = numbers.Emit({}, 2, Still());
    CHECK(numbers.Dismiss(dismissed));
    CHECK(numbers.Frame(dismissed)->phase == PopupPhase::Exit);
    CHECK_FALSE(numbers.Add(dismissed, 1));
    CHECK(numbers.Cancel(cancelled));
    CHECK_FALSE(numbers.IsAlive(cancelled));
    CHECK_FALSE(numbers.Value(cancelled));
    CHECK(numbers.ActiveCount() == 1);
}

TEST_CASE("NumberPopup.LabelsAnimateWithoutAValue", "[number-popup]") {
    NumberPopup numbers;
    const auto label = numbers.EmitLabel({}, "CRIT!", NumberPopupPreset::Burst, {.mergeKey = {7}});
    CHECK(numbers.IsAlive(label));
    CHECK_FALSE(numbers.Value(label));
    CHECK_FALSE(numbers.Add(label, 1));
    CHECK(numbers.Pulse(label));
    CHECK(numbers.ParticleCount() > 0);
    CHECK(numbers.Emit({}, 1, Still(), Keyed(7)) != label);
    CHECK(numbers.EmitLabel({}, "CRIT!", Still(), {.mergeKey = {7}}) != label);
}

TEST_CASE("NumberPopup.ParticlesFollowQualityAndDelayedActivation", "[number-popup]") {
    auto style = Still();
    style.juice.particleCount = 10;
    NumberPopup off;
    off.SetQuality({.particlePercent = 0});
    off.Emit({}, 1, style);
    CHECK(off.ParticleCount() == 0);

    NumberPopup delayed;
    style.popup.delaySeconds = 0.5f;
    delayed.Emit({}, 1, style);
    CHECK(delayed.ParticleCount() == 0);
    delayed.Update(0.625f);
    CHECK(delayed.ParticleCount() > 0);

    NumberPopup skipped;
    skipped.Emit({}, 1, style);
    skipped.Update(10.0f);
    CHECK(skipped.ActiveCount() == 0);
    CHECK(skipped.ParticleCount() == 0);
}

TEST_CASE("NumberPopup.FixedSeedReplaysWithinTheSameToolchain", "[number-popup]") {
    NumberPopup first({.seed = 42});
    NumberPopup second({.seed = 42});
    const auto style = NumberPopup::Preset(NumberPopupPreset::Burst);
    const auto a = first.Emit({}, 100, style);
    const auto b = second.Emit({}, 100, style);
    first.Update(0.25f);
    second.Update(0.25f);
    CHECK(first.Frame(a)->position == second.Frame(b)->position);
    CHECK(first.Frame(a)->rotation == second.Frame(b)->rotation);
}

TEST_CASE("NumberPopup.InvalidInputDoesNotEmitOrMerge", "[number-popup]") {
    NumberPopup numbers;
    const auto handle = numbers.Emit({}, 10, Still(), Keyed(1));
    auto invalid = Still();
    invalid.format.decimals = -1;
    CHECK_THROWS_AS(numbers.Emit({}, 20, invalid, Keyed(1)), std::invalid_argument);
    CHECK_THROWS_AS(numbers.Emit({}, std::numeric_limits<double>::infinity()), std::invalid_argument);
    CHECK_THROWS_AS(numbers.Emit({}, 1, Still(), {.intensity = -1.0f}), std::invalid_argument);
    CHECK(numbers.ActiveCount() == 1);
    CHECK(std::get<std::int64_t>(*numbers.Value(handle)) == 10);
}

TEST_CASE("NumberPopup.NonStrictSkipsAndCountsInvalidInput", "[number-popup]") {
    NumberPopup numbers({.strict = false});
    auto invalid = Still();
    invalid.juice.particles.lifetime = {0.0f, 0.0f};
    CHECK_FALSE(static_cast<bool>(numbers.Emit({}, 1, invalid)));
    CHECK_FALSE(static_cast<bool>(numbers.EmitLabel({}, "X", invalid)));
    const auto handle = numbers.Emit({}, 1, Still());
    CHECK_FALSE(numbers.Add(handle, std::numeric_limits<double>::quiet_NaN()));
    CHECK_FALSE(numbers.SetAnchor(handle, {std::numeric_limits<float>::infinity(), 0.0f}));
    CHECK(numbers.ActiveCount() == 1);
    CHECK(numbers.RejectedCount() == 4);
    CHECK_FALSE(numbers.FirstRejection().empty());
}

TEST_CASE("NumberPopup.EmitColorOverridesOnlyThatEntry", "[number-popup]") {
    NumberPopup numbers;
    auto style = NumberPopup::Preset({});
    const Color4 base = style.text.color;
    const auto handle = numbers.Emit({}, 1, style, {.color = Color4{1, 2, 3, 255}});
    CHECK(numbers.IsAlive(handle));
    CHECK(style.text.color == base);
    CHECK(numbers.Emit({}, 1, NumberPopupPreset::Heavy) != handle);
}
