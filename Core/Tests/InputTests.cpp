#include "Tests/Support/Diagnostics.h"
#include <initializer_list>
#include <type_traits>
#include "Input/InputNavigation.hpp"
#include "Input/Internals/InputNormalizer.h"

namespace {
DeviceInput Pressing(std::initializer_list<InputKey> keys) {
    DeviceInput input;
    for (const InputKey key : keys)
        input.keysPressed[static_cast<std::size_t>(key)] = true;
    return input;
}
std::optional<Vec2> MapPoint(Float2 point) {
    if (point.x < 0 || point.y < 0 || point.x >= 400 || point.y >= 300) return std::nullopt;
    return Vec2{static_cast<int>(point.x), static_cast<int>(point.y)};
}
} // namespace

static_assert(!std::is_constructible_v<InputNavigation, DeviceInput&&>);
static_assert(!std::is_constructible_v<InputNavigation, const DeviceInput&&>);

TEST_CASE("Engine.Input.NavigationMapsKeysAndBorrowsItsSnapshot", "[engine][input]") {
    DeviceInput input;
    const InputNavigation navigation(input);
    CHECK(&navigation.Device() == &input);
    CHECK(navigation.Vertical() == 0);
    CHECK(navigation.Horizontal() == 0);
    for (const InputKey key : {InputKey::W, InputKey::Up}) {
        input = Pressing({key});
        CHECK(navigation.Vertical() == -1);
        CHECK(navigation.Pressed(Direction::Up));
        CHECK_FALSE(navigation.Pressed(Direction::Down));
    }
    input = Pressing({InputKey::Down});
    CHECK(navigation.Vertical() == 1);
    input = Pressing({InputKey::Left});
    CHECK(navigation.Horizontal() == -1);
    input = Pressing({InputKey::D});
    CHECK(navigation.Horizontal() == 1);
    input = Pressing({InputKey::Space});
    CHECK(navigation.Confirm());
    input = Pressing({InputKey::Escape});
    CHECK(navigation.Back());
    input = Pressing({InputKey::Q});
    CHECK_FALSE(navigation.Back());
    CHECK(navigation.TabStep() == -1);
    input = Pressing({InputKey::Tab});
    CHECK(navigation.TabStep() == 1);
    input = {};
    input.keysDown[static_cast<std::size_t>(InputKey::RightShift)] = true;
    CHECK(input.ShiftDown());
    CHECK(navigation.AnyHeld());
    CHECK_FALSE(navigation.AnyPressed());
    input.pressPoints = {{50, 60}, {100, 120}};
    CHECK(navigation.PressPoint() == Vec2{50, 60});
    CHECK(navigation.PressPoints().data() == input.pressPoints.data());
}

TEST_CASE("Engine.Input.TouchBeginningsPreserveOutsideIdentityAndIndependentFingers", "[engine][input]") {
    InputNormalizer normalizer;
    InputSample sample;
    sample.touches = {{7, {-10, 20}}, {8, {50, 60}}};
    auto input = normalizer.Read(sample, MapPoint);
    REQUIRE(input.touches.size() == 2);
    CHECK(input.touches[0].id == 7);
    CHECK_FALSE(input.touches[0].position);
    REQUIRE(input.pressPoints.size() == 1);
    CHECK(input.pressPoints.front() == Vec2{50, 60});
    sample.touches = {{8, {60, 70}}, {7, {10, 20}}, {9, {80, 90}}};
    input = normalizer.Read(sample, MapPoint);
    REQUIRE(input.pressPoints.size() == 1);
    CHECK(input.pressPoints.front() == Vec2{80, 90});
    CHECK(input.heldPoints.size() == 3);
}

TEST_CASE("Engine.Input.TouchSuppressesSyntheticMouseUntilContactsAreQuiet", "[engine][input]") {
    InputNormalizer normalizer;
    InputSample sample;
    sample.mousePosition = {50, 60};
    sample.mousePressed = sample.mouseDown = sample.mouseMoved = true;
    sample.touches = {{1, {50, 60}}};
    auto input = normalizer.Read(sample, MapPoint);
    REQUIRE(input.pressPoints.size() == 1);
    CHECK_FALSE(input.mousePress);
    CHECK_FALSE(input.mouseHover);
    sample.touches.clear();
    sample.mouseDown = false;
    sample.mouseReleased = true;
    input = normalizer.Read(sample, MapPoint);
    CHECK(input.pressPoints.empty());
    CHECK_FALSE(input.mousePress);
    CHECK_FALSE(input.mouseHover);
    (void)normalizer.Read({}, MapPoint);
    sample.mouseReleased = false;
    sample.mouseDown = true;
    input = normalizer.Read(sample, MapPoint);
    CHECK(input.mousePress == Vec2{50, 60});
    CHECK(input.mouseHover == Vec2{50, 60});
}

TEST_CASE("Engine.Input.CancellationDrainsDevicesAndStillTracksContactIdentity", "[engine][input]") {
    InputNormalizer normalizer;
    InputSample sample;
    sample.keysDown[static_cast<std::size_t>(InputKey::Space)] = true;
    sample.touches = {{3, {50, 60}}};
    normalizer.Cancel();
    auto input = normalizer.Read(sample, MapPoint);
    CHECK(input.cancelled);
    CHECK(input.touches.empty());
    CHECK_FALSE(input.Down(InputKey::Space));
    CHECK(normalizer.Read(sample, MapPoint).cancelled);
    CHECK(normalizer.Read({}, MapPoint).cancelled);
    sample.keysPressed = sample.keysDown;
    input = normalizer.Read(sample, MapPoint);
    CHECK_FALSE(input.cancelled);
    CHECK(input.Pressed(InputKey::Space));
    REQUIRE(input.pressPoints.size() == 1);
    CHECK(input.pressPoints.front() == Vec2{50, 60});
}

TEST_CASE("Engine.Input.StationaryMouseDoesNotProduceNavigationHover", "[engine][input]") {
    InputNormalizer normalizer;
    InputSample sample;
    sample.mousePosition = {25, 35};
    auto input = normalizer.Read(sample, MapPoint);
    CHECK(input.mousePosition == Vec2{25, 35});
    CHECK_FALSE(input.mouseHover);
    sample.mouseMoved = true;
    CHECK(normalizer.Read(sample, MapPoint).mouseHover == Vec2{25, 35});
}
