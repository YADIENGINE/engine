#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <chrono>
#include <cstdio>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include "Graphics/Text/FontFace.h"
#include "Input/Input.h"
#include "Tests/PaintHarness.h"
#include "Ui/Kernel/Ui.h"
#include "Ui/Looks/Neon/NeonLooks.h"
#include "Ui/Looks/UiPanelLook.h"
#include "Ui/Widgets/UiButton.h"
#include "Ui/Widgets/UiLabel.h"
#include "Ui/Widgets/UiLayer.h"
#include "Ui/Widgets/UiPanel.h"

using Catch::Approx;

namespace {
constexpr float Frame = 1.0f / 60.0f;

const FontFace& TestFont() {
    static const FontFace font = FontFace::FromFile(ENGINE_TEST_FONT);
    return font;
}

// A Ui with a font on a 400 x 300 canvas, and what its screen pressed.
struct Screen {
    Screen()
        : ui(UiSettings{.canvas = {0.0f, 0.0f, 400.0f, 300.0f}, .font = &TestFont()}) {}

    // One frame with 'input'; 'build' says what is on screen.
    void Run(const std::function<void(Ui&)>& build, const DeviceInput& input = {}) {
        ui.Input(input);
        ui.Begin(Frame);
        build(ui);
        ui.End();
    }
    Ui ui;
};

[[nodiscard]] DeviceInput Pressed(InputKey key) {
    DeviceInput input;
    input.keysPressed[static_cast<std::size_t>(key)] = true;
    input.keysDown[static_cast<std::size_t>(key)] = true;
    return input;
}
[[nodiscard]] DeviceInput Held(InputKey key) {
    DeviceInput input;
    input.keysDown[static_cast<std::size_t>(key)] = true;
    return input;
}
[[nodiscard]] DeviceInput MouseMoved(int x, int y) {
    DeviceInput input;
    input.mousePosition = Vec2{x, y};
    input.mouseHover = Vec2{x, y};
    return input;
}
[[nodiscard]] DeviceInput MouseDown(int x, int y) {
    DeviceInput input;
    input.mousePosition = Vec2{x, y};
    input.mousePress = Vec2{x, y};
    input.mousePressed = true;
    input.mouseDown = true;
    return input;
}
[[nodiscard]] DeviceInput MouseUp(int x, int y) {
    DeviceInput input;
    input.mousePosition = Vec2{x, y};
    input.mouseReleased = true;
    return input;
}
// Fixed-size boxes in a column from the top left: each is 100 x 40, with no gap.
void Box(Ui& ui, UiId id, bool* activated = nullptr, bool* focused = nullptr, const UiTraits& extra = {}) {
    UiTraits traits = extra;
    traits.interact = UiInteract::Focus;
    UiElement element = ui.Element(id, {.width = UiFixed(100.0f), .height = UiFixed(40.0f)}, traits);
    if (activated) *activated = static_cast<bool>(element.Activated());
    if (focused) *focused = element.Focused();
}
} // namespace

// --- Pressing ---------------------------------------------------------------------------------------------------

TEST_CASE("Engine.Ui.AScreenStartsWithACursorOnItsFirstElement", "[engine][ui]") {
    Screen screen;
    bool first = false;
    bool second = false;
    const auto build = [&](Ui& ui) {
        Box(ui, "a", nullptr, &first);
        Box(ui, "b", nullptr, &second);
    };
    screen.Run(build);
    screen.Run(build);
    CHECK(first);
    CHECK_FALSE(second);
}

TEST_CASE("Engine.Ui.KeysMoveTheCursorAndConfirmPresses", "[engine][ui]") {
    Screen screen;
    std::array<bool, 3> activated{};
    std::array<bool, 3> focused{};
    const auto build = [&](Ui& ui) {
        Box(ui, "a", &activated[0], &focused[0]);
        Box(ui, "b", &activated[1], &focused[1]);
        Box(ui, "c", &activated[2], &focused[2]);
    };
    screen.Run(build);
    screen.Run(build, Pressed(InputKey::Down));
    CHECK(focused[1]);
    screen.Run(build, Pressed(InputKey::S)); // The default player has WASD too
    CHECK(focused[2]);
    screen.Run(build, Pressed(InputKey::Down));
    CHECK(focused[0]); // Wraps
    screen.Run(build, Pressed(InputKey::Up));
    CHECK(focused[2]);
    screen.Run(build, Pressed(InputKey::Enter));
    CHECK(activated[2]);
    CHECK_FALSE(activated[0]);
    screen.Run(build);
    CHECK_FALSE(activated[2]); // One press, one activation
}

TEST_CASE("Engine.Ui.AHeldDirectionRepeatsAfterADelay", "[engine][ui]") {
    Screen screen;
    std::array<bool, 3> focused{};
    int moves = 0;
    const auto build = [&](Ui& ui) {
        Box(ui, "a", nullptr, &focused[0]);
        Box(ui, "b", nullptr, &focused[1]);
        Box(ui, "c", nullptr, &focused[2]);
        for (const UiEvent& event : ui.Events()) moves += event.kind == UiEventKind::FocusMoved ? 1 : 0;
    };
    screen.Run(build);
    screen.Run(build, Pressed(InputKey::Down));
    CHECK(moves == 1);
    // 0.3 seconds: still under the delay.
    for (int frame = 0; frame < 18; ++frame) screen.Run(build, Held(InputKey::Down));
    CHECK(moves == 1);
    // 0.3 more: past the delay, repeating every 0.09.
    for (int frame = 0; frame < 18; ++frame) screen.Run(build, Held(InputKey::Down));
    CHECK(moves > 1);
    const int reached = moves;
    screen.Run(build);
    for (int frame = 0; frame < 30; ++frame) screen.Run(build);
    CHECK(moves == reached); // Letting go stops it
}

TEST_CASE("Engine.Ui.AClickNeedsPressAndReleaseOnTheSameElement", "[engine][ui]") {
    Screen screen;
    bool a = false;
    bool b = false;
    const auto build = [&](Ui& ui) {
        Box(ui, "a", &a);
        Box(ui, "b", &b);
    };
    screen.Run(build);
    screen.Run(build, MouseDown(10, 10));
    CHECK_FALSE(a); // Pressing alone does nothing yet
    screen.Run(build, MouseUp(10, 10));
    CHECK(a);

    screen.Run(build, MouseDown(10, 10));
    screen.Run(build, MouseUp(10, 50)); // Let go over the other one
    CHECK_FALSE(a);
    CHECK_FALSE(b);
}

TEST_CASE("Engine.Ui.MovingTheMouseMovesTheCursorButAStillMouseDoesNot", "[engine][ui]") {
    Screen screen;
    bool focusA = false;
    bool focusB = false;
    const auto build = [&](Ui& ui) {
        Box(ui, "a", nullptr, &focusA);
        Box(ui, "b", nullptr, &focusB);
    };
    screen.Run(build);
    screen.Run(build, MouseMoved(10, 50));
    CHECK(focusB);
    DeviceInput still;
    still.mousePosition = Vec2{10, 50};
    screen.Run(build, still);
    still.keysPressed[static_cast<std::size_t>(InputKey::Up)] = true;
    screen.Run(build, still);
    CHECK(focusA); // A resting mouse does not take the cursor back from the keyboard
}

TEST_CASE("Engine.Ui.ADisabledElementTakesTheCursorButIsDenied", "[engine][ui]") {
    Screen screen;
    bool activated = false;
    bool denied = false;
    int deniedEvents = 0;
    const auto build = [&](Ui& ui) {
        UiElement element = ui.Element("locked", {.width = UiFixed(100.0f), .height = UiFixed(40.0f)},
                                       {.interact = UiInteract::Focus, .enabled = false});
        activated = static_cast<bool>(element.Activated());
        denied = element.Denied();
        for (const UiEvent& event : ui.Events()) deniedEvents += event.kind == UiEventKind::Denied ? 1 : 0;
    };
    screen.Run(build);
    screen.Run(build, Pressed(InputKey::Enter));
    CHECK_FALSE(activated);
    CHECK(denied);
    CHECK(deniedEvents == 1);
}

TEST_CASE("Engine.Ui.AVisualMovesWhereClicksLand", "[engine][ui]") {
    Screen screen;
    bool activated = false;
    const auto build = [&](Ui& ui) {
        UiElement element = ui.Element("moved", {.width = UiFixed(100.0f), .height = UiFixed(40.0f)},
                                       {.interact = UiInteract::Focus, .visual = {.offset = {200.0f, 100.0f}}});
        activated = static_cast<bool>(element.Activated());
    };
    screen.Run(build);
    screen.Run(build, MouseDown(10, 10));
    screen.Run(build, MouseUp(10, 10));
    CHECK_FALSE(activated);
    screen.Run(build, MouseDown(210, 110));
    screen.Run(build, MouseUp(210, 110));
    CHECK(activated);
}

TEST_CASE("Engine.Ui.AClippedElementCannotBeClickedOutsideTheClip", "[engine][ui]") {
    Screen screen;
    bool activated = false;
    const auto build = [&](Ui& ui) {
        const UiElement window = ui.Element({.width = UiFixed(100.0f), .height = UiFixed(20.0f)}, {.clip = true});
        Box(ui, "inside", &activated);
    };
    screen.Run(build);
    screen.Run(build, MouseDown(10, 30));
    screen.Run(build, MouseUp(10, 30));
    CHECK_FALSE(activated);
    screen.Run(build, MouseDown(10, 10));
    screen.Run(build, MouseUp(10, 10));
    CHECK(activated);
}

TEST_CASE("Engine.Ui.ACancelledPressDoesNotClick", "[engine][ui]") {
    Screen screen;
    bool activated = false;
    const auto build = [&](Ui& ui) { Box(ui, "a", &activated); };
    screen.Run(build);
    screen.Run(build, MouseDown(10, 10));
    screen.ui.CancelInput();
    screen.Run(build, MouseUp(10, 10));
    CHECK_FALSE(activated);
}

TEST_CASE("Engine.Ui.AGoneElementSendsItsCursorToTheNearest", "[engine][ui]") {
    Screen screen;
    bool third = false;
    bool withSecond = true;
    bool focusB = false;
    bool focusC = false;
    const auto build = [&](Ui& ui) {
        Box(ui, "a");
        if (withSecond) Box(ui, "b", nullptr, &focusB);
        Box(ui, "c", nullptr, &focusC);
        third = focusC;
    };
    screen.Run(build);
    screen.Run(build, Pressed(InputKey::Down));
    CHECK(focusB);
    withSecond = false;
    focusB = false;
    screen.Run(build);
    screen.Run(build);
    CHECK(third); // "c" moved up into the place of "b"
}

// --- Layers -----------------------------------------------------------------------------------------------------

TEST_CASE("Engine.Ui.AModalLayerKeepsInputFromWhatIsBelow", "[engine][ui]") {
    Screen screen;
    bool open = false;
    bool below = false;
    bool above = false;
    bool focusBelow = false;
    const auto build = [&](Ui& ui) {
        Box(ui, "below", &below, &focusBelow);
        if (auto layer = ui.Layer("dialog", open, {.seconds = 0.0f})) Box(ui, "above", &above);
    };
    screen.Run(build);
    screen.Run(build);
    CHECK(focusBelow);
    open = true;
    screen.Run(build);
    screen.Run(build);
    CHECK_FALSE(focusBelow); // The cursor moved up to the layer
    screen.Run(build, Pressed(InputKey::Enter));
    CHECK(above);
    CHECK_FALSE(below);
    // Both are at the same place: the mouse reaches only the top.
    above = false;
    screen.Run(build, MouseDown(10, 10));
    screen.Run(build, MouseUp(10, 10));
    CHECK(above);
    CHECK_FALSE(below);
    // A click outside the layer's elements reaches nothing.
    above = false;
    screen.Run(build, MouseDown(300, 200));
    screen.Run(build, MouseUp(300, 200));
    CHECK_FALSE(above);
}

TEST_CASE("Engine.Ui.ClosingALayerGivesTheCursorBack", "[engine][ui]") {
    Screen screen;
    bool open = false;
    bool focusSecond = false;
    const auto build = [&](Ui& ui) {
        Box(ui, "first");
        Box(ui, "second", nullptr, &focusSecond);
        if (auto layer = ui.Layer("dialog", open, {.seconds = 0.0f})) Box(ui, "above");
    };
    screen.Run(build);
    screen.Run(build, Pressed(InputKey::Down));
    CHECK(focusSecond);
    open = true;
    screen.Run(build);
    screen.Run(build);
    CHECK_FALSE(focusSecond);
    open = false;
    screen.Run(build);
    screen.Run(build);
    CHECK(focusSecond);
}

TEST_CASE("Engine.Ui.ALayerStaysUntilItsClosingIsDoneButTakesNoInput", "[engine][ui]") {
    Screen screen;
    bool open = true;
    bool described = false;
    float progress = 0.0f;
    bool closing = false;
    bool activated = false;
    const auto build = [&](Ui& ui) {
        described = false;
        if (auto layer = ui.Layer("dialog", open, {.seconds = 0.1f})) {
            described = true;
            progress = layer.Open();
            closing = layer.Closing();
            Box(ui, "button", &activated);
        }
    };
    for (int frame = 0; frame < 3; ++frame) screen.Run(build);
    CHECK(progress > 0.0f);
    CHECK(progress < 1.0f);
    for (int frame = 0; frame < 10; ++frame) screen.Run(build);
    CHECK(progress == Approx(1.0f));
    open = false;
    screen.Run(build, Pressed(InputKey::Enter));
    CHECK(described);
    CHECK(closing);
    CHECK_FALSE(activated);
    for (int frame = 0; frame < 10; ++frame) screen.Run(build);
    CHECK_FALSE(described);
}

TEST_CASE("Engine.Ui.BackGoesToTheTopLayer", "[engine][ui]") {
    Screen screen;
    bool open = true;
    bool baseBack = false;
    bool layerBack = false;
    const auto build = [&](Ui& ui) {
        baseBack = static_cast<bool>(ui.Back());
        if (auto layer = ui.Layer("dialog", open, {.seconds = 0.0f})) {
            layerBack = static_cast<bool>(ui.Back());
            Box(ui, "button");
        }
    };
    screen.Run(build);
    screen.Run(build, Pressed(InputKey::Escape));
    CHECK(layerBack);
    CHECK_FALSE(baseBack);
    open = false;
    layerBack = false;
    screen.Run(build);
    screen.Run(build, Pressed(InputKey::Escape));
    CHECK(baseBack);
    CHECK_FALSE(layerBack);
}

TEST_CASE("Engine.Ui.ALayerWithoutModalLeavesWhatIsBelowWorking", "[engine][ui]") {
    Screen screen;
    bool below = false;
    const auto build = [&](Ui& ui) {
        Box(ui, "below", &below);
        if (auto toast = ui.Layer("toast", true, {.seconds = 0.0f, .modal = false})) {
            UiLabel(ui, "Saved", {.anchor = UiAnchor::BottomRight});
        }
    };
    screen.Run(build);
    screen.Run(build);
    screen.Run(build, Pressed(InputKey::Enter));
    CHECK(below);
}

// --- Players ----------------------------------------------------------------------------------------------------

TEST_CASE("Engine.Ui.EachPlayerHasTheirOwnCursor", "[engine][ui]") {
    Screen screen;
    screen.ui.SetUsers({UiKeys::Wasd(), UiKeys::Arrows()});
    std::array<std::array<bool, 2>, 2> focused{};
    std::array<UiAction, 2> actions{};
    const auto build = [&](Ui& ui) {
        for (int index = 0; index < 2; ++index) {
            UiElement element = ui.Element(index, {.width = UiFixed(100.0f), .height = UiFixed(40.0f)},
                                           {.interact = UiInteract::Focus});
            const auto slot = static_cast<std::size_t>(index);
            focused[0][slot] = element.Focused(0);
            focused[1][slot] = element.Focused(1);
            actions[slot] = element.Activated();
        }
    };
    screen.Run(build);
    screen.Run(build, Pressed(InputKey::S));
    CHECK(focused[0][1]); // The first player moved
    CHECK(focused[1][0]); // The second did not
    screen.Run(build, Pressed(InputKey::Enter));
    CHECK(static_cast<bool>(actions[0])); // The second player's confirm presses what they are on
    CHECK(actions[0].user == 1);
    CHECK_FALSE(static_cast<bool>(actions[1]));
}

TEST_CASE("Engine.Ui.AnOwnedAreaIgnoresTheOtherPlayerAndTheMouse", "[engine][ui]") {
    Screen screen;
    screen.ui.SetUsers({UiKeys::Wasd(), UiKeys::Arrows()});
    bool leftFocus = false;
    bool rightFocus = false;
    bool leftActivated = false;
    const auto build = [&](Ui& ui) {
        const UiElement row = UiRow(ui);
        {
            const UiElement left = ui.Element("p1", {}, {.owner = 0});
            UiElement button = ui.Element("button", {.width = UiFixed(100.0f), .height = UiFixed(40.0f)},
                                          {.interact = UiInteract::Focus});
            leftFocus = button.Focused(1); // The second player must never get here
            leftActivated = static_cast<bool>(button.Activated());
        }
        {
            const UiElement right = ui.Element("p2", {}, {.owner = 1});
            UiElement button = ui.Element("button", {.width = UiFixed(100.0f), .height = UiFixed(40.0f)},
                                          {.interact = UiInteract::Focus});
            rightFocus = button.Focused(1);
        }
    };
    screen.Run(build);
    screen.Run(build);
    CHECK_FALSE(leftFocus);
    CHECK(rightFocus);
    // Rule for several players: a mouse does not reach an element a player owns.
    screen.Run(build, MouseDown(10, 10));
    screen.Run(build, MouseUp(10, 10));
    CHECK_FALSE(leftActivated);
    screen.Run(build, Pressed(InputKey::Space));
    CHECK(leftActivated);
}

TEST_CASE("Engine.Ui.TheMouseWorksInASharedAreaWithSeveralPlayers", "[engine][ui]") {
    Screen screen;
    screen.ui.SetUsers({UiKeys::Wasd(), UiKeys::Arrows()});
    bool activated = false;
    const auto build = [&](Ui& ui) { Box(ui, "shared", &activated); };
    screen.Run(build);
    screen.Run(build, MouseDown(10, 10));
    screen.Run(build, MouseUp(10, 10));
    CHECK(activated);
}

// --- Elements ---------------------------------------------------------------------------------------------------

TEST_CASE("Engine.Ui.ARememberedValueLivesAsLongAsItsElement", "[engine][ui]") {
    Screen screen;
    bool shown = true;
    int seen = 0;
    float age = 0.0f;
    const auto build = [&](Ui& ui) {
        if (!shown) return;
        UiElement element = ui.Element("counter");
        seen = ++element.Remember<int>(100);
        age = element.Age();
    };
    screen.Run(build);
    screen.Run(build);
    CHECK(seen == 102);
    CHECK(age == Approx(Frame));
    shown = false;
    screen.Run(build);
    shown = true;
    screen.Run(build);
    CHECK(seen == 101); // It started over
    CHECK(age == 0.0f);
}

TEST_CASE("Engine.Ui.TwoElementsWithOneIdAreAnError", "[engine][ui]") {
    Screen screen;
    screen.ui.Begin(Frame);
    { const UiElement first = screen.ui.Element("same"); }
    CHECK_THROWS_AS(screen.ui.Element("same"), std::logic_error);
}

TEST_CASE("Engine.Ui.UnnamedElementsAreToldApartByTheirPlace", "[engine][ui]") {
    Screen screen;
    const auto build = [&](Ui& ui) {
        UiLabel(ui, "Same");
        UiLabel(ui, "Same");
    };
    CHECK_NOTHROW(screen.Run(build));
}

TEST_CASE("Engine.Ui.ScopesEndingOutOfOrderAreReportedAtTheEnd", "[engine][ui]") {
    Screen screen;
    screen.ui.Begin(Frame);
    UiElement first = screen.ui.Element("first");
    UiElement second = screen.ui.Element("second");
    first.Close();
    CHECK_THROWS_AS(screen.ui.End(), std::logic_error);
}

TEST_CASE("Engine.Ui.ElementsOutsideAFrameAreAnError", "[engine][ui]") {
    Screen screen;
    CHECK_THROWS_AS(screen.ui.Element("outside"), std::logic_error);
    CHECK_THROWS_AS(screen.ui.End(), std::logic_error);
}

TEST_CASE("Engine.Ui.TextWithoutAFontSaysSo", "[engine][ui]") {
    Ui ui;
    ui.Begin(Frame);
    CHECK_THROWS_AS(UiLabel(ui, "text"), std::logic_error);
}

// --- Widgets, and what a game adds ------------------------------------------------------------------------------

namespace {
// A game's own widget and look: it uses nothing but the public API, as the engine's widgets do.
struct GameSlotLook {
    Color4 tint{1, 2, 3, 255};
    std::function<void(Painter&, const UiDrawState&, Color4)> draw = [](Painter&, const UiDrawState&, Color4) {};
};
struct SlotMemory {
    int presses = 0;
};
int GameSlot(Ui& ui, UiId id) {
    const GameSlotLook& look = ui.Looks().Get<GameSlotLook>();
    UiElement slot =
        ui.Element(id, {.width = UiFixed(64.0f), .height = UiFixed(64.0f)}, {.interact = UiInteract::Focus});
    SlotMemory& memory = slot.Remember<SlotMemory>();
    if (slot.Activated()) ++memory.presses;
    slot.Draw([look = &look](Painter& paint, const UiDrawState& state) { look->draw(paint, state, look->tint); });
    return memory.presses;
}
} // namespace

TEST_CASE("Engine.Ui.AGameWritesItsOwnWidgetAndLookWithThePublicApi", "[engine][ui]") {
    Screen screen;
    int drawn = 0;
    Color4 tint{};
    FloatRect rect{};
    GameSlotLook custom{.tint = {9, 8, 7, 255}};
    custom.draw = [&](Painter&, const UiDrawState& state, Color4 color) {
        ++drawn;
        tint = color;
        rect = state.rect;
    };
    screen.ui.Looks().Set(custom);
    int presses = 0;
    const auto build = [&](Ui& ui) { presses = GameSlot(ui, "slot"); };
    screen.Run(build);
    screen.Run(build, Pressed(InputKey::Enter));
    CHECK(presses == 1);
    PaintFrame([&](Painter& paint) { screen.ui.Draw(paint); });
    CHECK(drawn == 1);
    CHECK(tint == Color4{9, 8, 7, 255});
    CHECK(rect.width == Approx(64.0f));
}

TEST_CASE("Engine.Ui.ChangingALookChangesNoWidgetCode", "[engine][ui]") {
    Screen screen;
    int panels = 0;
    UiPanelLook counted;
    counted.draw = [&](Painter&, const UiPanelView&) { ++panels; };
    const auto build = [&](Ui& ui) {
        auto panel = UiPanel(ui, "panel", {.anchor = UiAnchor::Center});
        UiLabel(ui, "Title");
        static_cast<void>(UiButton(ui, "ok", "OK"));
    };
    screen.Run(build);
    PaintFrame([&](Painter& paint) { screen.ui.Draw(paint); });
    CHECK(panels == 0);

    screen.ui.Looks().Set(counted);
    screen.Run(build);
    PaintFrame([&](Painter& paint) { screen.ui.Draw(paint); });
    CHECK(panels == 1);
}

TEST_CASE("Engine.Ui.ButtonsFitTheirTextAndAPanelFitsThem", "[engine][ui]") {
    Screen screen;
    FloatRect button{};
    const auto build = [&](Ui& ui) {
        auto panel = UiPanel(ui, "panel", {.anchor = UiAnchor::Center});
        static_cast<void>(UiButton(ui, "wide", "A rather long label for a button"));
        {
            UiElement probe = ui.Element("probe", {.width = UiFixed(10.0f), .height = UiFixed(10.0f)});
            button = probe.LastRect();
        }
    };
    screen.Run(build);
    screen.Run(build);
    CHECK(button.x > 0.0f); // Inside the centered panel
    CHECK(button.y > 0.0f);
}

TEST_CASE("Engine.Ui.ALookDoesNotOverruleTheRoomAButtonIsGiven", "[engine][ui]") {
    Screen screen;
    UiButtonLook wide;
    wide.minWidth = 500.0f;
    screen.ui.Looks().Set(wide);
    FloatRect filled{};
    FloatRect fitted{};
    const auto build = [&](Ui& ui) {
        {
            const UiElement row = UiRow(ui, {.width = UiFixed(300.0f)});
            static_cast<void>(UiButton(ui, "filled", "A", {.width = UiFill()}));
            UiElement probe = ui.Element("probe");
            filled = probe.LastRect();
        }
        static_cast<void>(UiButton(ui, "fitted", "B"));
        UiElement after = ui.Element("after");
        fitted = after.LastRect();
    };
    screen.Run(build);
    screen.Run(build);
    CHECK(filled.x == Approx(300.0f)); // The filled button took the row, not 500
    CHECK(fitted.y > filled.y);
}

TEST_CASE("Engine.Ui.PlainAndNeonLooksDrawEveryWidget", "[engine][ui]") {
    const auto drawAll = [](Ui& ui) {
        PaintHarness harness;
        harness.context.font = &TestFont();
        harness.Begin();
        DeviceInput none;
        ui.Input(none);
        ui.Begin(Frame);
        {
            auto panel = UiPanel(ui, "panel", {.anchor = UiAnchor::Center});
            UiLabel(ui, "Title", {.size = 30.0f});
            UiLabel(ui, "A long text that has to wrap at the panel's edge", {.wrap = true, .width = UiFixed(100.0f)});
            static_cast<void>(UiButton(ui, "ok", "OK"));
            static_cast<void>(UiButton(ui, "no", "Locked", {.enabled = false}));
        }
        if (auto layer = UiLayer(ui, "layer", true)) UiLabel(ui, "On top");
        ui.End();
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

TEST_CASE("Engine.Ui.DrawingTwiceOrBeforeAFrameIsSafe", "[engine][ui]") {
    Screen screen;
    PaintFrame([&](Painter& paint) { screen.ui.Draw(paint); });
    screen.Run([](Ui& ui) { UiLabel(ui, "Text"); });
    PaintFrame([&](Painter& paint) {
        screen.ui.Draw(paint);
        screen.ui.Draw(paint);
    });
}

TEST_CASE("Engine.Ui.EventsTellWhatHappenedForSoundsAndEffects", "[engine][ui]") {
    Screen screen;
    std::map<UiEventKind, int> counts;
    bool open = false;
    const auto build = [&](Ui& ui) {
        Box(ui, "a");
        Box(ui, "b");
        if (auto layer = ui.Layer("dialog", open, {.seconds = 0.0f})) Box(ui, "c");
        for (const UiEvent& event : ui.Events()) ++counts[event.kind];
    };
    screen.Run(build);
    CHECK(counts.empty()); // A cursor placed by the screen itself makes no sound
    screen.Run(build, Pressed(InputKey::Down));
    CHECK(counts[UiEventKind::FocusMoved] == 1);
    screen.Run(build, Pressed(InputKey::Enter));
    CHECK(counts[UiEventKind::Activated] == 1);
    screen.Run(build, Pressed(InputKey::Escape));
    CHECK(counts[UiEventKind::Back] == 1);
    open = true;
    screen.Run(build);
    CHECK(counts[UiEventKind::LayerOpened] == 1);
    open = false;
    screen.Run(build);
    CHECK(counts[UiEventKind::LayerClosed] == 1);
}

// Not a test of behaviour: run with "[.ui-perf]" in Release to read the cost of a large screen.
TEST_CASE("Engine.Ui.CostOfALargeScreen", "[.ui-perf]") {
    Screen screen;
    bool volume = false;
    const auto build = [&](Ui& ui) {
        auto panel = UiPanel(ui, "panel", {.width = UiFill(), .height = UiFill()});
        for (int row = 0; row < 30; ++row) {
            const UiElement line = UiRow(ui, {.width = UiFill(), .gap = 4.0f});
            UiLabel(ui, "A label on a row");
            for (int column = 0; column < 8; ++column) {
                if (UiButton(ui, row * 100 + column, "Button")) volume = !volume;
            }
        }
    };
    PaintHarness harness;
    harness.context.font = &TestFont();
    const auto now = [] { return std::chrono::steady_clock::now(); };
    const auto micros = [](auto from, auto to) {
        return std::chrono::duration<double, std::micro>(to - from).count();
    };
    constexpr int Warmup = 60;
    constexpr int Frames = 600;
    double build_ = 0.0;
    double draw = 0.0;
    for (int frame = 0; frame < Warmup + Frames; ++frame) {
        const auto start = now();
        screen.Run(build, Pressed(frame % 2 == 0 ? InputKey::Down : InputKey::Right));
        const auto built = now();
        harness.Begin();
        screen.ui.Draw(harness.paint);
        const auto recorded = now();
        harness.End();
        if (frame >= Warmup) {
            build_ += micros(start, built);
            draw += micros(built, recorded);
        }
    }
    const UiStats& stats = screen.ui.Stats();
    std::printf("elements %d, interactive %d: build + layout %.0f us, recording draws %.0f us per frame\n",
                stats.elements, stats.interactive, build_ / Frames, draw / Frames);
    CHECK(stats.elements > 300);
}
