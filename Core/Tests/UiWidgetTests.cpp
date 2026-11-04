#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "Graphics/Text/FontFace.h"
#include "Input/Input.h"
#include "Tests/PaintHarness.h"
#include "Ui/Kernel/Ui.h"
#include "Ui/Kit/UiOptionsScreen.h"
#include "Ui/Layout/Internals/UiLayoutSolver.h"
#include "Ui/Looks/Neon/NeonLooks.h"
#include "Ui/Looks/UiSliderLook.h"
#include "Ui/Looks/UiTabLook.h"
#include "Ui/Looks/UiTooltipLook.h"
#include "Ui/Widgets/UiButton.h"
#include "Ui/Widgets/UiLabel.h"
#include "Ui/Widgets/UiPanel.h"
#include "Ui/Widgets/UiScroll.h"
#include "Ui/Widgets/UiSlider.h"
#include "Ui/Widgets/UiStepper.h"
#include "Ui/Widgets/UiTabs.h"
#include "Ui/Widgets/UiToggle.h"
#include "Ui/Widgets/UiTooltip.h"

using Catch::Approx;

namespace {
constexpr float Frame = 1.0f / 60.0f;

const FontFace& TestFont() {
    static const FontFace font = FontFace::FromFile(ENGINE_TEST_FONT);
    return font;
}
struct Screen {
    Screen()
        : ui(UiSettings{.canvas = {0.0f, 0.0f, 800.0f, 600.0f}, .font = &TestFont()}) {}
    void Run(const std::function<void(Ui&)>& build, const DeviceInput& input = {}) {
        ui.Input(input);
        ui.Begin(Frame);
        build(ui);
        ui.End();
    }
    void Settle(const std::function<void(Ui&)>& build, int frames = 3) {
        for (int frame = 0; frame < frames; ++frame) Run(build);
    }
    Ui ui;
};
[[nodiscard]] DeviceInput Pressed(InputKey key) {
    DeviceInput input;
    input.keysPressed[static_cast<std::size_t>(key)] = true;
    input.keysDown[static_cast<std::size_t>(key)] = true;
    return input;
}
[[nodiscard]] DeviceInput MouseDown(float x, float y) {
    DeviceInput input;
    input.mousePosition = Vec2{static_cast<int>(x), static_cast<int>(y)};
    input.mousePress = input.mousePosition;
    input.mousePressed = true;
    input.mouseDown = true;
    return input;
}
[[nodiscard]] DeviceInput MouseHeld(float x, float y) {
    DeviceInput input;
    input.mousePosition = Vec2{static_cast<int>(x), static_cast<int>(y)};
    input.mouseDown = true;
    return input;
}
[[nodiscard]] DeviceInput MouseUp(float x, float y) {
    DeviceInput input;
    input.mousePosition = Vec2{static_cast<int>(x), static_cast<int>(y)};
    input.mouseReleased = true;
    return input;
}
[[nodiscard]] DeviceInput Wheel(float x, float y, float amount) {
    DeviceInput input;
    input.mousePosition = Vec2{static_cast<int>(x), static_cast<int>(y)};
    input.wheel = amount;
    return input;
}
} // namespace

// --- Grid ---------------------------------------------------------------------------------------------------------

TEST_CASE("Engine.Ui.GridPlacesChildrenInColumnsAndRows", "[engine][ui]") {
    std::vector<UiLayoutNode> nodes(6);
    nodes[0].box = {.direction = UiDirection::Grid, .columns = 2, .gap = 10.0f, .padding = UiInsets::All(5.0f)};
    nodes[0].firstChild = 1;
    nodes[0].lastChild = 5;
    for (int index = 1; index < 6; ++index) {
        auto& node = nodes[static_cast<std::size_t>(index)];
        node.parent = 0;
        node.nextSibling = index < 5 ? index + 1 : -1;
        node.box = {.width = UiFill(), .height = UiFixed(index == 2 ? 50.0f : 30.0f)};
    }
    SolveUiLayout(nodes, {0.0f, 0.0f, 210.0f, 300.0f});
    CHECK(nodes[1].rect.x == Approx(5.0f));
    CHECK(nodes[1].rect.width == Approx(95.0f));
    CHECK(nodes[2].rect.x == Approx(110.0f));
    CHECK(nodes[3].rect.y == Approx(65.0f)); // Below the row's tallest
    CHECK(nodes[5].rect.y == Approx(105.0f));
}

TEST_CASE("Engine.Ui.AGridThatFitsItsContentCountsRowsAndColumns", "[engine][ui]") {
    Screen screen;
    FloatRect grid{};
    const auto build = [&](Ui& ui) {
        {
            const UiElement tiles = UiGrid(ui, 3, {.gap = 2.0f});
            for (int index = 0; index < 4; ++index) {
                const UiElement tile = ui.Element(index, {.width = UiFixed(10.0f), .height = UiFixed(10.0f)});
            }
        }
        UiElement after = ui.Element("after");
        grid = after.LastRect();
    };
    screen.Settle(build);
    CHECK(grid.y == Approx(22.0f)); // Two rows and a gap
}

TEST_CASE("Engine.Ui.TheCursorMovesThroughAGridByItsShape", "[engine][ui]") {
    Screen screen;
    std::array<bool, 6> focused{};
    const auto build = [&](Ui& ui) {
        const UiElement tiles = UiGrid(ui, 3, {.gap = 2.0f});
        for (int index = 0; index < 6; ++index) {
            UiElement tile = ui.Element(index, {.width = UiFixed(40.0f), .height = UiFixed(40.0f)},
                                        {.interact = UiInteract::Focus});
            focused[static_cast<std::size_t>(index)] = tile.Focused();
        }
    };
    screen.Settle(build);
    CHECK(focused[0]);
    screen.Run(build, Pressed(InputKey::Right));
    screen.Run(build, Pressed(InputKey::Down));
    CHECK(focused[4]);
    screen.Run(build, Pressed(InputKey::Down));
    CHECK(focused[1]); // Wrapped within its column
}

// --- Controls -----------------------------------------------------------------------------------------------------

TEST_CASE("Engine.Ui.AToggleFlipsAndLeftRightSetIt", "[engine][ui]") {
    Screen screen;
    bool value = false;
    bool changed = false;
    bool other = false;
    const auto build = [&](Ui& ui) {
        changed = UiToggle(ui, "toggle", "Sound", value);
        UiElement next = ui.Element("next", {.width = UiFixed(10.0f), .height = UiFixed(10.0f)},
                                    {.interact = UiInteract::Focus});
        other = next.Focused();
    };
    screen.Settle(build);
    screen.Run(build, Pressed(InputKey::Enter));
    CHECK(value);
    CHECK(changed);
    screen.Run(build);
    CHECK_FALSE(changed);
    screen.Run(build, Pressed(InputKey::Right));
    CHECK(value); // Already on
    CHECK_FALSE(changed);
    screen.Run(build, Pressed(InputKey::Left));
    CHECK_FALSE(value);
    CHECK(changed);
    CHECK_FALSE(other); // Left and right stayed with the row
    screen.Run(build, Pressed(InputKey::Down));
    CHECK(other); // Up and down still move the cursor
}

TEST_CASE("Engine.Ui.ASliderStepsByKeysAndStopsAtItsEnds", "[engine][ui]") {
    Screen screen;
    int value = 50;
    bool changed = false;
    const auto build = [&](Ui& ui) { changed = UiSlider(ui, "volume", "Volume", value, 0, 100, 10); };
    screen.Settle(build);
    screen.Run(build, Pressed(InputKey::Right));
    CHECK(value == 60);
    CHECK(changed);
    screen.Run(build);
    CHECK_FALSE(changed);
    value = 95;
    screen.Run(build, Pressed(InputKey::Right));
    CHECK(value == 100);
    screen.Run(build, Pressed(InputKey::Right));
    CHECK_FALSE(changed);
}

TEST_CASE("Engine.Ui.ASliderDragsOnlyFromItsControl", "[engine][ui]") {
    Screen screen;
    float value = 0.5f;
    const UiSliderLook look;
    FloatRect row{};
    const auto build = [&](Ui& ui) {
        static_cast<void>(UiSlider(ui, "s", "Label", value, 0.0f, 1.0f, 0.1f));
        UiElement probe = ui.Element("probe");
        row = FloatRect{0.0f, 0.0f, 800.0f, probe.LastRect().y};
    };
    screen.Settle(build);
    const FloatRect track = UiSliderTrack(row, look);
    const float y = row.height * 0.5f;
    // On the label: nothing moves.
    screen.Run(build, MouseDown(20.0f, y));
    screen.Run(build, MouseUp(20.0f, y));
    CHECK(value == Approx(0.5f));
    // On the track's far end, and dragged back to its start.
    screen.Run(build, MouseDown(track.Right() - 1.0f, y));
    CHECK(value == Approx(1.0f));
    screen.Run(build, MouseHeld(track.x, y));
    CHECK(value == Approx(0.0f));
    screen.Run(build, MouseUp(track.x, y));
}

TEST_CASE("Engine.Ui.ASliderOverFloatsKeepsToItsSteps", "[engine][ui]") {
    Screen screen;
    float value = 0.0f;
    const auto build = [&](Ui& ui) { static_cast<void>(UiSlider(ui, "s", "Label", value, 0.0f, 1.0f, 0.25f)); };
    screen.Settle(build);
    screen.Run(build, Pressed(InputKey::Right));
    CHECK(value == Approx(0.25f));
}

TEST_CASE("Engine.Ui.ASliderNeedsARange", "[engine][ui]") {
    Screen screen;
    int value = 0;
    screen.ui.Begin(Frame);
    CHECK_THROWS_AS(static_cast<void>(UiSlider(screen.ui, "s", "Label", value, 5, 5)), std::invalid_argument);
}

TEST_CASE("Engine.Ui.ADisabledControlIsDeniedAndDoesNotChange", "[engine][ui]") {
    Screen screen;
    bool value = false;
    int number = 5;
    int denied = 0;
    const auto build = [&](Ui& ui) {
        static_cast<void>(UiToggle(ui, "t", "T", value, {.enabled = false}));
        static_cast<void>(UiSlider(ui, "s", "S", number, 0, 10, 1, {.enabled = false}));
        for (const UiEvent& event : ui.Events()) denied += event.kind == UiEventKind::Denied ? 1 : 0;
    };
    screen.Settle(build);
    screen.Run(build, Pressed(InputKey::Enter));
    screen.Run(build, Pressed(InputKey::Right));
    screen.Run(build, Pressed(InputKey::Down));
    screen.Run(build, Pressed(InputKey::Left));
    CHECK_FALSE(value);
    CHECK(number == 5);
    CHECK(denied == 3);
}

TEST_CASE("Engine.Ui.AStepperGivesStepsAndWrapsAList", "[engine][ui]") {
    Screen screen;
    int selected = 0;
    bool changed = false;
    const std::array<std::string_view, 3> names{"One", "Two", "Three"};
    const auto build = [&](Ui& ui) { changed = UiStepper(ui, "s", "Choice", selected, names); };
    screen.Settle(build);
    screen.Run(build, Pressed(InputKey::Right));
    CHECK(selected == 1);
    CHECK(changed);
    screen.Run(build, Pressed(InputKey::Enter));
    CHECK(selected == 2);
    screen.Run(build, Pressed(InputKey::Enter));
    CHECK(selected == 0); // Wraps
    screen.Run(build, Pressed(InputKey::Left));
    CHECK(selected == 2);
    screen.Run(build);
    CHECK_FALSE(changed);
}

TEST_CASE("Engine.Ui.ClickingAStepperArrowGoesThatWay", "[engine][ui]") {
    Screen screen;
    int steps = 0;
    const auto build = [&](Ui& ui) { steps += UiStepper(ui, "s", "Choice", "Value"); };
    screen.Settle(build);
    // The control is at the row's right end, 220 wide with 14 of padding: 566..786.
    screen.Run(build, MouseDown(580.0f, 20.0f));
    screen.Run(build, MouseUp(580.0f, 20.0f));
    CHECK(steps == -1);
    screen.Run(build, MouseDown(770.0f, 20.0f));
    screen.Run(build, MouseUp(770.0f, 20.0f));
    CHECK(steps == 0);
    screen.Run(build, MouseDown(50.0f, 20.0f));
    screen.Run(build, MouseUp(50.0f, 20.0f));
    CHECK(steps == 1); // The rest of the row is forward
}

TEST_CASE("Engine.Ui.AKeyConfirmOnAStepperIgnoresARestingMouse", "[engine][ui]") {
    Screen screen;
    int steps = 0;
    const auto build = [&](Ui& ui) { steps += UiStepper(ui, "s", "Choice", "Value"); };
    screen.Settle(build);
    DeviceInput input = Pressed(InputKey::Enter);
    input.mousePosition = Vec2{580, 20}; // Resting on the left arrow
    screen.Run(build, input);
    CHECK(steps == 1);
}

// --- Tabs ---------------------------------------------------------------------------------------------------------

TEST_CASE("Engine.Ui.TabsAreOneCursorStopWhereLeftAndRightSwitch", "[engine][ui]") {
    Screen screen;
    int page = 0;
    bool changed = false;
    bool rowBelow = false;
    const std::array<std::string_view, 3> names{"A", "B", "C"};
    const auto build = [&](Ui& ui) {
        changed = UiTabs(ui, "tabs", names, page);
        UiElement below = ui.Element("below", {.width = UiFixed(100.0f), .height = UiFixed(40.0f)},
                                     {.interact = UiInteract::Focus});
        rowBelow = below.Focused();
    };
    screen.Settle(build);
    CHECK_FALSE(rowBelow);
    screen.Run(build, Pressed(InputKey::Right));
    CHECK(page == 1);
    CHECK(changed);
    screen.Run(build, Pressed(InputKey::Left));
    screen.Run(build, Pressed(InputKey::Left));
    CHECK(page == 2); // Wraps
    screen.Run(build, Pressed(InputKey::Down));
    CHECK(rowBelow);
    CHECK(page == 2); // Going down leaves the choice alone
    screen.Run(build, Pressed(InputKey::Up));
    CHECK_FALSE(rowBelow); // And up comes back to the row, whichever tab is chosen
}

TEST_CASE("Engine.Ui.PageKeysSwitchTabsWhereverTheCursorIs", "[engine][ui]") {
    Screen screen;
    int page = 0;
    const std::array<std::string_view, 2> names{"A", "B"};
    const auto build = [&](Ui& ui) {
        static_cast<void>(UiTabs(ui, "tabs", names, page));
        const UiElement below = ui.Element("below", {.width = UiFixed(100.0f), .height = UiFixed(40.0f)},
                                           {.interact = UiInteract::Focus});
    };
    screen.Settle(build);
    screen.Run(build, Pressed(InputKey::Down));
    screen.Run(build, Pressed(InputKey::E));
    CHECK(page == 1);
    screen.Run(build, Pressed(InputKey::Q));
    CHECK(page == 0);
}

TEST_CASE("Engine.Ui.ClickingATabChoosesItAndTakesTheCursor", "[engine][ui]") {
    Screen screen;
    int page = 0;
    const std::array<std::string_view, 2> names{"A", "B"};
    bool below = false;
    const auto build = [&](Ui& ui) {
        static_cast<void>(UiTabs(ui, "tabs", names, page));
        UiElement under = ui.Element("under", {.width = UiFixed(100.0f), .height = UiFixed(40.0f)},
                                     {.interact = UiInteract::Focus});
        below = under.Focused();
    };
    screen.Settle(build);
    screen.Run(build, Pressed(InputKey::Down));
    CHECK(below);
    // The second tab starts after the first, which is at least the look's 72 wide, and the 4 of gap.
    screen.Run(build, MouseDown(90.0f, 10.0f));
    screen.Run(build, MouseUp(90.0f, 10.0f));
    CHECK(page == 1);
    CHECK_FALSE(below);
}

TEST_CASE("Engine.Ui.TabsDrawTheirChoiceEvenWhenALaterTabWasClicked", "[engine][ui]") {
    Screen screen;
    int page = 0;
    const std::array<std::string_view, 2> names{"A", "B"};
    screen.ui.Looks().Set(UiTabLook{});
    const auto build = [&](Ui& ui) { static_cast<void>(UiTabs(ui, "tabs", names, page)); };
    screen.Settle(build);
    screen.Run(build, MouseDown(90.0f, 10.0f));
    screen.Run(build, MouseUp(90.0f, 10.0f));
    int selectedDrawn = -1;
    int drawn = 0;
    UiTabLook counting;
    counting.draw = [&](Painter&, const UiTabView& view) {
        if (view.selected) selectedDrawn = drawn;
        ++drawn;
    };
    screen.ui.Looks().Set(counting);
    screen.Run(build);
    PaintFrame([&](Painter& paint) { screen.ui.Draw(paint); });
    CHECK(drawn == 2);
    CHECK(selectedDrawn == 1);
}

// --- Scrolling ----------------------------------------------------------------------------------------------------

namespace {
// Twenty rows 40 high in a view 200 high: 600 can scroll.
struct ScrollCase {
    Screen screen;
    float offset = 0.0f;
    std::array<bool, 20> focused{};
    std::array<bool, 20> activated{};
    void Build(Ui& ui) {
        UiScroll list(ui, "list", {}, {.width = UiFixed(300.0f), .height = UiFixed(200.0f)});
        offset = list.Offset();
        for (int index = 0; index < 20; ++index) {
            UiElement row = ui.Element(index, {.width = UiFill(), .height = UiFixed(40.0f)},
                                       {.interact = UiInteract::Focus});
            focused[static_cast<std::size_t>(index)] = row.Focused();
            activated[static_cast<std::size_t>(index)] = static_cast<bool>(row.Activated());
        }
    }
    void Run(const DeviceInput& input = {}) {
        screen.Run([this](Ui& ui) { Build(ui); }, input);
    }
    void Settle(int frames = 90) {
        for (int frame = 0; frame < frames; ++frame) Run();
    }
};
} // namespace

TEST_CASE("Engine.Ui.TheCursorReachesRowsOutOfViewAndTheListFollows", "[engine][ui]") {
    ScrollCase list;
    list.Settle(5);
    CHECK(list.offset == 0.0f);
    for (int press = 0; press < 7; ++press) list.Run(Pressed(InputKey::Down));
    CHECK(list.focused[7]); // Below the view when it was pressed to
    list.Settle();
    // Row 7 ends at 320; 200 of view with the margin of 8 shows it.
    CHECK(list.offset == Approx(128.0f).margin(1.0f));
    for (int press = 0; press < 7; ++press) list.Run(Pressed(InputKey::Up));
    list.Settle();
    CHECK(list.focused[0]);
    CHECK(list.offset == Approx(0.0f).margin(1.0f));
}

TEST_CASE("Engine.Ui.TheWheelScrollsOnlyOverTheList", "[engine][ui]") {
    ScrollCase list;
    list.Settle(5);
    list.Run(Wheel(100.0f, 100.0f, -2.0f));
    list.Settle();
    CHECK(list.offset == Approx(120.0f).margin(1.0f)); // Two notches of 60
    list.Run(Wheel(600.0f, 100.0f, -2.0f));
    list.Settle();
    CHECK(list.offset == Approx(120.0f).margin(1.0f));
    list.Run(Wheel(100.0f, 100.0f, 50.0f));
    list.Settle();
    CHECK(list.offset == Approx(0.0f).margin(1.0f)); // Stops at the top
    list.Run(Wheel(100.0f, 100.0f, -50.0f));
    list.Settle();
    CHECK(list.offset == Approx(600.0f).margin(1.0f)); // And at the bottom
}

TEST_CASE("Engine.Ui.AfterTheWheelTheListDoesNotJumpBackToTheCursor", "[engine][ui]") {
    ScrollCase list;
    list.Settle(5);
    list.Run(Wheel(100.0f, 100.0f, -2.0f));
    list.Settle();
    CHECK(list.focused[0]); // Out of view, and left alone
    CHECK(list.offset == Approx(120.0f).margin(1.0f));
}

TEST_CASE("Engine.Ui.ARowClickedAfterScrollingIsTheOneUnderTheMouse", "[engine][ui]") {
    ScrollCase list;
    list.Settle(5);
    list.Run(Wheel(100.0f, 100.0f, -2.0f)); // 120 down: row 3 starts at y 0
    list.Settle();
    DeviceInput down;
    down.mousePosition = Vec2{50, 10};
    down.mousePress = down.mousePosition;
    down.mouseDown = true;
    list.Run(down);
    DeviceInput up;
    up.mousePosition = Vec2{50, 10};
    up.mouseReleased = true;
    list.Run(up);
    CHECK(list.activated[3]);
}

TEST_CASE("Engine.Ui.RowsScrolledOutOfViewCannotBeClicked", "[engine][ui]") {
    ScrollCase list;
    list.Settle(5);
    // Row 5 is laid out at y 200, just below the view of 200, and is clipped.
    list.Run(MouseDown(50.0f, 210.0f));
    list.Run(MouseUp(50.0f, 210.0f));
    for (const bool activated : list.activated) CHECK_FALSE(activated);
}

TEST_CASE("Engine.Ui.DraggingTheBarScrolls", "[engine][ui]") {
    ScrollCase list;
    list.Settle(5);
    // The bar is at the right edge, 8 wide, from x 292 to 300.
    list.Run(MouseDown(296.0f, 5.0f));
    list.Run(MouseHeld(296.0f, 199.0f));
    list.Settle(2);
    CHECK(list.offset > 500.0f);
}

TEST_CASE("Engine.Ui.ContentThatFitsDoesNotScroll", "[engine][ui]") {
    Screen screen;
    float offset = -1.0f;
    const auto build = [&](Ui& ui) {
        UiScroll list(ui, "list", {}, {.height = UiFixed(200.0f)});
        offset = list.Offset();
        UiLabel(ui, "One line");
    };
    screen.Settle(build);
    screen.Run(build, Wheel(100.0f, 100.0f, -5.0f));
    screen.Settle(build);
    CHECK(offset == 0.0f);
}

TEST_CASE("Engine.Ui.AScrollingGridFollowsTheCursor", "[engine][ui]") {
    Screen screen;
    float offset = 0.0f;
    const auto build = [&](Ui& ui) {
        UiScroll grid(ui, "grid", {.direction = UiDirection::Grid, .columns = 4, .gap = 4.0f},
                      {.width = UiFixed(400.0f), .height = UiFixed(100.0f)});
        offset = grid.Offset();
        for (int index = 0; index < 40; ++index) {
            const UiElement tile = ui.Element(index, {.width = UiFill(), .height = UiFixed(40.0f)},
                                              {.interact = UiInteract::Focus});
        }
    };
    screen.Settle(build);
    for (int press = 0; press < 5; ++press) screen.Run(build, Pressed(InputKey::Down));
    for (int frame = 0; frame < 90; ++frame) screen.Run(build);
    CHECK(offset > 100.0f);
}

// --- Tooltips -----------------------------------------------------------------------------------------------------

TEST_CASE("Engine.Ui.ATooltipAppearsAfterRestingAndGoesAway", "[engine][ui]") {
    Screen screen;
    const auto build = [](Ui& ui) {
        const auto press = UiButton(ui, "first", "First", {.tooltip = "What it does"});
        static_cast<void>(press);
        const auto other = UiButton(ui, "second", "Second");
        static_cast<void>(other);
    };
    screen.Run(build);
    const int plain = screen.ui.Stats().elements;
    // The keyboard cursor rests on the first button: half a second, and a tooltip has elements of its own.
    for (int frame = 0; frame < 25; ++frame) screen.Run(build);
    CHECK(screen.ui.Stats().elements == plain);
    for (int frame = 0; frame < 10; ++frame) screen.Run(build);
    CHECK(screen.ui.Stats().elements > plain);
    screen.Run(build, Pressed(InputKey::Down));
    CHECK(screen.ui.Stats().elements == plain);
}

TEST_CASE("Engine.Ui.ATooltipWithoutFocusNeedsTheMouse", "[engine][ui]") {
    Screen screen;
    const auto build = [](Ui& ui) {
        UiElement element = ui.Element("e", {.width = UiFixed(100.0f), .height = UiFixed(40.0f)},
                                       {.interact = UiInteract::Focus});
        UiTooltip(ui, element, "Words", {.onFocus = false});
    };
    screen.Run(build);
    const int plain = screen.ui.Stats().elements;
    for (int frame = 0; frame < 60; ++frame) screen.Run(build);
    CHECK(screen.ui.Stats().elements == plain);
    DeviceInput over;
    over.mousePosition = Vec2{10, 10};
    for (int frame = 0; frame < 60; ++frame) screen.Run(build, over);
    CHECK(screen.ui.Stats().elements > plain);
}

TEST_CASE("Engine.Ui.ATooltipStaysInsideTheCanvas", "[engine][ui]") {
    Screen screen;
    FloatRect tip{};
    UiTooltipLook look;
    look.draw = [&](Painter&, const UiDrawState& state) { tip = state.rect; };
    screen.ui.Looks().Set(look);
    // In the bottom right corner, so it must go above it and to the left.
    const auto build = [](Ui& ui) {
        const UiBox corner{.width = UiFixed(40.0f), .height = UiFixed(40.0f), .anchor = UiAnchor::BottomRight};
        UiElement element = ui.Element("e", corner, {.interact = UiInteract::Focus});
        UiTooltip(ui, element, "A tooltip that is much wider than its element");
    };
    for (int frame = 0; frame < 60; ++frame) screen.Run(build);
    PaintFrame([&](Painter& paint) { screen.ui.Draw(paint); });
    CHECK(tip.width > 40.0f);
    CHECK(tip.x >= 0.0f);
    CHECK(tip.Right() <= 800.0f);
    CHECK(tip.Bottom() <= 560.0f);
}

TEST_CASE("Engine.Ui.ATooltipDoesNotDisturbFocusOrBack", "[engine][ui]") {
    Screen screen;
    bool back = false;
    bool focused = false;
    int layerEvents = 0;
    const auto build = [&](Ui& ui) {
        UiElement element = ui.Element("e", {.width = UiFixed(100.0f), .height = UiFixed(40.0f)},
                                       {.interact = UiInteract::Focus});
        UiTooltip(ui, element, "Words");
        focused = element.Focused();
        back = static_cast<bool>(ui.Back());
        for (const UiEvent& event : ui.Events()) {
            if (event.kind == UiEventKind::LayerOpened || event.kind == UiEventKind::LayerClosed) ++layerEvents;
        }
    };
    for (int frame = 0; frame < 60; ++frame) screen.Run(build);
    CHECK(focused);
    screen.Run(build, Pressed(InputKey::Escape));
    CHECK(back);
    CHECK(layerEvents == 0);
}

// --- Options screen -------------------------------------------------------------------------------------------------

TEST_CASE("Engine.Ui.TheOptionsScreenChangesValuesAndCloses", "[engine][ui]") {
    Screen screen;
    UiOptionsValues values;
    UiOptionsResult result;
    const auto build = [&](Ui& ui) { result = UiOptionsScreen(ui, "options", values); };
    screen.Settle(build);
    // The cursor starts on the tabs; the first row of the Audio tab is music.
    screen.Run(build, Pressed(InputKey::Down));
    screen.Settle(build);
    screen.Run(build, Pressed(InputKey::Left));
    CHECK(values.musicPercent == 95);
    CHECK(result.changed);
    screen.Run(build);
    CHECK_FALSE(result.changed);
    CHECK_FALSE(result.closed);
    screen.Run(build, Pressed(InputKey::Escape));
    CHECK(result.closed);
}

TEST_CASE("Engine.Ui.TheOptionsScreenSwitchesTabs", "[engine][ui]") {
    Screen screen;
    UiOptionsValues values;
    const auto build = [&](Ui& ui) { static_cast<void>(UiOptionsScreen(ui, "options", values)); };
    screen.Settle(build);
    screen.Run(build, Pressed(InputKey::E)); // Display
    screen.Settle(build);
    screen.Run(build, Pressed(InputKey::Down));
    screen.Settle(build);
    screen.Run(build, Pressed(InputKey::Enter));
    CHECK(values.fullscreen);
    screen.Run(build, Pressed(InputKey::Down));
    screen.Settle(build);
    screen.Run(build, Pressed(InputKey::Right));
    CHECK(values.fpsLimit == 120);
    screen.Run(build, Pressed(InputKey::Left));
    CHECK(values.fpsLimit == 60);
}

TEST_CASE("Engine.Ui.TheOptionsScreenTakesAGamesOwnRowsAndWords", "[engine][ui]") {
    Screen screen;
    UiOptionsValues values;
    bool hints = false;
    UiOptionsConfig config;
    config.audio = false;
    config.display = false;
    config.effects = false;
    config.extraTab = "Game";
    config.texts.back = "Geri";
    config.extraRows = [&](Ui& ui) { static_cast<void>(UiToggle(ui, "hints", "Hints", hints)); };
    const auto build = [&](Ui& ui) { static_cast<void>(UiOptionsScreen(ui, "options", values, config)); };
    screen.Settle(build);
    screen.Run(build, Pressed(InputKey::Down));
    screen.Settle(build);
    screen.Run(build, Pressed(InputKey::Enter));
    CHECK(hints);
}

TEST_CASE("Engine.Ui.EveryNewWidgetDrawsInBothLooks", "[engine][ui]") {
    const auto drawAll = [](Ui& ui) {
        PaintHarness harness;
        harness.context.font = &TestFont();
        harness.Begin();
        UiOptionsValues values;
        const std::array<std::string_view, 2> names{"A", "B"};
        for (int page = 0; page < 3; ++page) {
            ui.Begin(Frame);
            {
                auto panel = UiPanel(ui, "panel", {.width = UiFill()});
                bool toggle = true;
                int number = 3;
                int tab = 0;
                static_cast<void>(UiToggle(ui, "t", "Toggle", toggle, {.tooltip = "Words"}));
                static_cast<void>(UiSlider(ui, "s", "Slider", number, 0, 10));
                static_cast<void>(UiStepper(ui, "st", "Stepper", "Value"));
                static_cast<void>(UiTabs(ui, "tabs", names, tab));
                UiScroll list(ui, "list", {}, {.height = UiFixed(60.0f)});
                for (int row = 0; row < 8; ++row) UiLabel(ui, "A row");
            }
            static_cast<void>(UiOptionsScreen(ui, "options", values));
            ui.End();
        }
        ui.Draw(harness.paint);
        harness.End();
        return harness.Draws().size();
    };
    Screen plain;
    CHECK(drawAll(plain.ui) > 0);
    Screen neon;
    neon.ui.Looks() = NeonLooks();
    CHECK(drawAll(neon.ui) > 0);
}
