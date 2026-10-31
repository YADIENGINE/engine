#include "Tests/Support/Diagnostics.h"
#include "Input/CollectionPointer.h"
#include "Input/Input.h"

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
    mouse.mousePosition = Vec2{155, 205};
    mouse.mouseReleased = true;
    mouse.mouseHover = mouse.mousePosition;
    CHECK_FALSE(pointer.Update(mouse).tap);
    CHECK_FALSE(pointer.Update(mouse).hover);
    (void)pointer.Update({});
    mouse.mouseReleased = false;
    CHECK(pointer.Update(mouse).hover == mouse.mousePosition);
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
TEST_CASE("Engine.CollectionPointer.CancelDiscardsHeldContactAndItsRelease", "[engine][input]") {
    CollectionPointer pointer(Area, 40);
    (void)pointer.Update(Finger({150, 220}));
    pointer.Cancel();
    CHECK(pointer.Update(Finger({150, 100})).rows == 0);
    CHECK_FALSE(pointer.Update({}).tap);
    (void)pointer.Update(Finger({150, 220}));
    CHECK(pointer.Update({}).tap == Vec2{150, 220});
}
TEST_CASE("Engine.CollectionPointer.MouseReleaseAndWheelArea", "[engine][input]") {
    CollectionPointer pointer(Area, 40);
    DeviceInput mouse;
    mouse.mousePosition = Vec2{150, 200};
    mouse.mousePressed = mouse.mouseDown = true;
    CHECK_FALSE(pointer.Update(mouse).tap);
    mouse.mousePressed = mouse.mouseDown = false;
    mouse.mouseReleased = true;
    CHECK(pointer.Update(mouse).tap == mouse.mousePosition);
    mouse.mouseReleased = false;
    mouse.wheel = 1.0f;
    CHECK(pointer.Update(mouse).rows == -1);
    mouse.mousePosition = Vec2{10, 10};
    CHECK(pointer.Update(mouse).rows == 0);
}

TEST_CASE("Engine.CollectionPointer.CancelledSnapshotDoesNotReleaseATap", "[engine][input]") {
    CollectionPointer pointer(Area, 40);
    (void)pointer.Update(Finger({150, 200}));
    CHECK_FALSE(pointer.Update(DeviceInput{.cancelled = true}).tap);
    (void)pointer.Update(Finger({155, 205}));
    CHECK(pointer.Update({}).tap == Vec2{155, 205});
}

TEST_CASE("Engine.CollectionPointer.LeavingCanvasCannotReactivateOnReentry", "[engine][input]") {
    CollectionPointer pointer(Area, 40);
    DeviceInput input;
    input.mousePosition = Vec2{150, 200};
    input.mousePressed = input.mouseDown = true;
    (void)pointer.Update(input);
    input.mousePressed = false;
    input.mousePosition.reset();
    CHECK(pointer.Update(input).cancelled);
    input.mousePosition = Vec2{150, 200};
    CHECK(pointer.Update(input).cancelled);
    input.mouseDown = false;
    input.mouseReleased = true;
    CHECK_FALSE(pointer.Update(input).tap);
    (void)pointer.Update({});
    input.mousePressed = input.mouseDown = true;
    input.mouseReleased = false;
    (void)pointer.Update(input);
    input.mousePressed = input.mouseDown = false;
    input.mouseReleased = true;
    CHECK(pointer.Update(input).tap == Vec2{150, 200});
}
