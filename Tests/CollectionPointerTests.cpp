#include "Tests/Support/Diagnostics.h"
#include "Input/CollectionPointer.h"
#include "Input/DeviceInput.h"

namespace {
constexpr Rect Area{100, 100, 200, 200};
DeviceInput Finger(Vec2 point, int id = 1) {
    DeviceInput input;
    input.touches.push_back({id, point});
    return input;
}
} // namespace

TEST_CASE("Engine.CollectionPointer.ReleaseTapAndSyntheticMouseSuppression", "[engine][input]") {
    CollectionPointer pointer(Area, 40);
    CHECK_FALSE(pointer.Update(Finger({150, 200})).tap);
    CHECK_FALSE(pointer.Update(Finger({155, 205})).tap);
    CHECK(pointer.Update({}).tap == Vec2{155, 205});
    DeviceInput mouse;
    mouse.mouse = Vec2{155, 205};
    mouse.mouseReleased = true;
    mouse.hover = mouse.mouse;
    CHECK_FALSE(pointer.Update(mouse).tap);
    CHECK_FALSE(pointer.Update(mouse).hover);
    (void)pointer.Update({});
    mouse.mouseReleased = false;
    CHECK(pointer.Update(mouse).hover == mouse.mouse);
}
TEST_CASE("Engine.CollectionPointer.DragReturningToOriginNeverActivates", "[engine][input]") {
    CollectionPointer pointer(Area, 40);
    (void)pointer.Update(Finger({150, 220}));
    CHECK(pointer.Update(Finger({150, 170})).rows == 1);
    CHECK(pointer.Update(Finger({150, 220})).rows == -1);
    CHECK_FALSE(pointer.Update({}).tap);
}
TEST_CASE("Engine.CollectionPointer.PrimaryIdentityAndOutsideCancellation", "[engine][input]") {
    CollectionPointer pointer(Area, 40);
    (void)pointer.Update(Finger({150, 220}, 7));
    auto input = Finger({150, 100}, 8);
    input.touches.push_back({7, Vec2{150, 210}});
    CHECK(pointer.Update(input).rows == 0);
    input.touches[1].position.reset();
    CHECK_FALSE(pointer.Update(input).tap);
    input.touches[1].position = Vec2{150, 210};
    (void)pointer.Update(input);
    CHECK_FALSE(pointer.Update({}).tap);
}
TEST_CASE("Engine.CollectionPointer.ResetCancelsHeldContactAndItsRelease", "[engine][input]") {
    CollectionPointer pointer(Area, 40);
    (void)pointer.Update(Finger({150, 220}));
    pointer.Reset();
    CHECK(pointer.Update(Finger({150, 100})).rows == 0);
    CHECK_FALSE(pointer.Update({}).tap);
    (void)pointer.Update(Finger({150, 220}));
    CHECK(pointer.Update({}).tap == Vec2{150, 220});
}
TEST_CASE("Engine.CollectionPointer.MouseReleaseAndWheelArea", "[engine][input]") {
    CollectionPointer pointer(Area, 40);
    DeviceInput mouse;
    mouse.mouse = Vec2{150, 200};
    mouse.mousePressed = mouse.mouseDown = true;
    CHECK_FALSE(pointer.Update(mouse).tap);
    mouse.mousePressed = mouse.mouseDown = false;
    mouse.mouseReleased = true;
    CHECK(pointer.Update(mouse).tap == mouse.mouse);
    mouse.mouseReleased = false;
    mouse.wheel = 1.0f;
    CHECK(pointer.Update(mouse).rows == -1);
    mouse.mouse = Vec2{10, 10};
    CHECK(pointer.Update(mouse).rows == 0);
}
