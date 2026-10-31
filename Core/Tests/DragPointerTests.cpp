#include "Tests/Support/Diagnostics.h"
#include "Input/Input.h"
#include "Input/DragPointer.h"

namespace {
constexpr int Threshold = 12;
DeviceInput Finger(Vec2 point, int id = 1) {
    DeviceInput input;
    input.touches.push_back({id, point});
    return input;
}
DeviceInput Mouse(Vec2 point, bool pressed, bool down, bool released) {
    DeviceInput input;
    input.mousePosition = point;
    input.mousePressed = pressed;
    input.mouseDown = down;
    input.mouseReleased = released;
    return input;
}
} // namespace

TEST_CASE("Engine.DragPointer.MouseDragReportsOriginPositionAndRelease", "[engine][input]") {
    DragPointer pointer(Threshold);
    auto result = pointer.Update(Mouse({100, 100}, true, true, false));
    CHECK(result.pressed == Vec2{100, 100});
    CHECK(result.position == Vec2{100, 100});
    CHECK_FALSE(result.dragging);
    result = pointer.Update(Mouse({105, 100}, false, true, false));
    CHECK_FALSE(result.dragging);
    result = pointer.Update(Mouse({160, 140}, false, true, false));
    CHECK(result.dragging);
    CHECK(result.origin == Vec2{100, 100});
    CHECK(result.position == Vec2{160, 140});
    result = pointer.Update(Mouse({170, 150}, false, false, true));
    CHECK(result.released == Vec2{170, 150});
    CHECK(result.dragging);
    CHECK_FALSE(result.tap);
    CHECK_FALSE(result.position.has_value());
    // Sonraki frame boşta: hover geçer.
    DeviceInput idle;
    idle.mouseHover = Vec2{20, 20};
    CHECK(pointer.Update(idle).hover == Vec2{20, 20});
}

TEST_CASE("Engine.DragPointer.TouchTapIgnoresSyntheticMouse", "[engine][input]") {
    DragPointer pointer(Threshold);
    CHECK(pointer.Update(Finger({50, 60})).pressed == Vec2{50, 60});
    CHECK_FALSE(pointer.Update(Finger({52, 61})).dragging);
    const auto released = pointer.Update({});
    CHECK(released.tap);
    CHECK(released.released == Vec2{52, 61});
    // Dokunuşun ürettiği sahte fare bırakması yeni bir temas sayılmaz.
    const auto synthetic = pointer.Update(Mouse({52, 61}, true, false, true));
    CHECK_FALSE(synthetic.pressed);
    CHECK_FALSE(synthetic.released);
}

TEST_CASE("Engine.DragPointer.SecondFingerDoesNotStealTheContact", "[engine][input]") {
    DragPointer pointer(Threshold);
    (void)pointer.Update(Finger({50, 50}, 3));
    auto input = Finger({300, 300}, 4);
    input.touches.push_back({3, Vec2{90, 50}});
    const auto result = pointer.Update(input);
    CHECK(result.position == Vec2{90, 50});
    CHECK(result.dragging);
    CHECK(pointer.Update(Finger({300, 300}, 4)).released == Vec2{90, 50});
}

TEST_CASE("Engine.DragPointer.CancelDiscardsTheContactAndItsRelease", "[engine][input]") {
    DragPointer pointer(Threshold);
    (void)pointer.Update(Mouse({100, 100}, true, true, false));
    pointer.Cancel();
    CHECK_FALSE(pointer.Update(Mouse({160, 100}, false, true, false)).position.has_value());
    CHECK_FALSE(pointer.Update(Mouse({160, 100}, false, false, true)).released.has_value());
    (void)pointer.Update({});
    CHECK(pointer.Update(Mouse({10, 10}, true, true, false)).pressed == Vec2{10, 10});
}

TEST_CASE("Engine.DragPointer.CancelledSnapshotCannotActivateTheOwnedContact", "[engine][input]") {
    DragPointer pointer(Threshold);
    (void)pointer.Update(Mouse({100, 100}, true, true, false));
    const auto cancelled = pointer.Update(DeviceInput{.cancelled = true});
    CHECK(cancelled.cancelled);
    CHECK_FALSE(cancelled.tap);
    CHECK_FALSE(cancelled.released);
    CHECK(pointer.Update(Mouse({20, 30}, true, true, false)).pressed == Vec2{20, 30});
}

TEST_CASE("Engine.DragPointer.TouchStartingOutsideCannotBecomeATap", "[engine][input]") {
    DragPointer pointer(Threshold);
    DeviceInput outside;
    outside.touches.push_back({1, std::nullopt});
    CHECK(pointer.Update(outside).cancelled);
    const auto entered = pointer.Update(Finger({50, 60}));
    CHECK_FALSE(entered.pressed);
    CHECK_FALSE(entered.position);
    const auto released = pointer.Update({});
    CHECK_FALSE(released.tap);
    CHECK_FALSE(released.released);
    CHECK(pointer.Update(Finger({50, 60})).pressed == Vec2{50, 60});
}

TEST_CASE("Engine.DragPointer.CoalescedMouseEdgesKeepThePressOrigin", "[engine][input]") {
    DragPointer pointer(Threshold);
    auto input = Mouse({200, 100}, true, false, true);
    input.mousePress = Vec2{100, 100};
    const auto result = pointer.Update(input);
    CHECK(result.pressed == Vec2{100, 100});
    CHECK(result.origin == Vec2{100, 100});
    CHECK(result.released == Vec2{200, 100});
    CHECK(result.dragging);
    CHECK_FALSE(result.tap);
}

TEST_CASE("Engine.DragPointer.HeldCancellationCannotActivateOnRelease", "[engine][input]") {
    DragPointer pointer(Threshold);
    (void)pointer.Update(Finger({50, 60}, 7));
    auto input = Finger({55, 65}, 7);
    input.cancelled = true;
    CHECK(pointer.Update(input).cancelled);
    input.cancelled = false;
    CHECK(pointer.Update(input).cancelled);
    CHECK_FALSE(pointer.Update({}).tap);
    CHECK(pointer.Update(Finger({100, 120}, 8)).pressed == Vec2{100, 120});
}

TEST_CASE("Engine.DragPointer.LeavingCanvasCancelsInsteadOfDroppingAtTheLastPosition", "[engine][input]") {
    DragPointer pointer(Threshold);
    SECTION("Mouse") {
        (void)pointer.Update(Mouse({100, 100}, true, true, false));
        DeviceInput outside;
        outside.mouseDown = true;
        const auto cancelled = pointer.Update(outside);
        CHECK(cancelled.cancelled);
        CHECK_FALSE(cancelled.position);
        outside.mouseDown = false;
        outside.mouseReleased = true;
        const auto released = pointer.Update(outside);
        CHECK_FALSE(released.tap);
        CHECK_FALSE(released.released);
    }
    SECTION("Touch cannot resume after reentering") {
        (void)pointer.Update(Finger({100, 100}));
        (void)pointer.Update(Finger({200, 100}));
        DeviceInput outside;
        outside.touches.push_back({1, std::nullopt});
        CHECK(pointer.Update(outside).cancelled);
        CHECK_FALSE(pointer.Update(Finger({200, 100})).position);
        const auto released = pointer.Update({});
        CHECK_FALSE(released.tap);
        CHECK_FALSE(released.released);
    }
}
