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
    style.merge.maxLifetimeSeconds = 1.5f;
    return style;
}
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

TEST_CASE("NumberPopup.AddPreservesIntegerPrecisionAndRejectsOverflowAtomically", "[number-popup]") {
    NumberPopup numbers;
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto handle = numbers.Emit({}, maximum - 2, Still());
    CHECK(numbers.Add(handle, 2));
    REQUIRE(numbers.Value(handle));
    CHECK(std::get<std::int64_t>(*numbers.Value(handle)) == maximum);
    CHECK_THROWS_AS(numbers.Add(handle, 1), std::overflow_error);
    CHECK(std::get<std::int64_t>(*numbers.Value(handle)) == maximum);
    numbers.Clear();
    CHECK_FALSE(numbers.Value(handle));
    CHECK_FALSE(numbers.Add(handle, 1));
}

TEST_CASE("NumberPopup.MergesOnlyCompatibleKeysWithinTheWindow", "[number-popup]") {
    NumberPopup numbers;
    auto style = Still();
    const auto first = numbers.Emit({20.0f, 30.0f}, 120, style, 1.0f, {42});
    numbers.Update(0.125f);
    const auto before = *numbers.Frame(first);
    const auto merged = numbers.Emit({200.0f, 300.0f}, 30, style, 1.0f, {42});
    CHECK(merged == first);
    CHECK(std::get<std::int64_t>(*numbers.Value(first)) == 150);
    CHECK(numbers.Frame(first)->position == before.position);
    CHECK(numbers.Frame(first)->ageSeconds == before.ageSeconds);
    CHECK(numbers.Emit({}, -10, style, 1.0f, {42}) != first);
    CHECK(numbers.Emit({}, 10, style, 1.0f, {43}) != first);
    CHECK(numbers.Emit({}, 10, style) != first);
    style.merge.windowSeconds = 0.0f;
    CHECK(numbers.Emit({}, 10, style, 1.0f, {42}) != first);
    numbers.Update(0.5f);
    CHECK(numbers.Emit({}, 10, Still(), 1.0f, {42}) != first);
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
    const auto handle = numbers.Emit({}, 10, Still(), 1.0f, {1});
    auto invalid = Still();
    invalid.format.decimals = -1;
    CHECK_THROWS_AS(numbers.Emit({}, 20, invalid, 1.0f, {1}), std::invalid_argument);
    CHECK_THROWS_AS(numbers.Emit({}, std::numeric_limits<double>::infinity()), std::invalid_argument);
    CHECK_THROWS_AS(numbers.Emit({}, 1, Still(), -1.0f), std::invalid_argument);
    CHECK(numbers.ActiveCount() == 1);
    CHECK(std::get<std::int64_t>(*numbers.Value(handle)) == 10);
}
