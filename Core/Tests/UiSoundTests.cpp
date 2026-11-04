#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <array>
#include <functional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "Audio/SoundOutput.h"
#include "Audio/SoundPlayer.h"
#include "Audio/SoundTables.h"
#include "Graphics/Text/FontFace.h"
#include "Input/Input.h"
#include "Ui/Kernel/Ui.h"
#include "Ui/Kernel/UiListener.h"
#include "Ui/Kit/UiOptionsScreen.h"
#include "Ui/Sound/UiSoundListener.h"
#include "Ui/Sound/UiSounds.h"
#include "Ui/Widgets/UiButton.h"
#include "Ui/Widgets/UiLayer.h"
#include "Ui/Widgets/UiScroll.h"
#include "Ui/Widgets/UiSlider.h"
#include "Ui/Widgets/UiStepper.h"
#include "Ui/Widgets/UiTabs.h"
#include "Ui/Widgets/UiToggle.h"

namespace {
constexpr float Frame = 1.0f / 60.0f;

// Every row may overlap, as a table's default does: the "no overlap" of moves must not depend on the row.
enum class Sfx { Move, Step, Confirm, Back, Deny, Open, Close, Other, Count };
enum class Stranger { Nobody, Count };
constexpr std::array SfxTable{
    SoundRow<Sfx>{Sfx::Move, {"move.wav"}},       SoundRow<Sfx>{Sfx::Step, {"step.wav"}},
    SoundRow<Sfx>{Sfx::Confirm, {"confirm.wav"}}, SoundRow<Sfx>{Sfx::Back, {"back.wav"}},
    SoundRow<Sfx>{Sfx::Deny, {"deny.wav"}},       SoundRow<Sfx>{Sfx::Open, {"open.wav"}},
    SoundRow<Sfx>{Sfx::Close, {"close.wav"}},     SoundRow<Sfx>{Sfx::Other, {"other.wav"}},
};
const UiSounds AllSounds{.move = Sfx::Move,
                         .step = Sfx::Step,
                         .confirm = Sfx::Confirm,
                         .back = Sfx::Back,
                         .deny = Sfx::Deny,
                         .open = Sfx::Open,
                         .close = Sfx::Close};

[[nodiscard]] const FontFace& TestFont() {
    static const FontFace font = FontFace::FromFile(ENGINE_TEST_FONT);
    return font;
}

// The device: what would have been heard, in order. Plays until told otherwise, like a sound longer than a frame.
class FakeOutput final : public SoundOutput {
public:
    [[nodiscard]] bool IsPlaying(SoundId id) const override { return _playing.contains(Row(id)); }
    void Play(SoundId id, float) override {
        heard.push_back(static_cast<Sfx>(Row(id)));
        _playing.insert(Row(id));
    }
    void Finish() { _playing.clear(); }
    std::vector<Sfx> heard;

private:
    [[nodiscard]] static std::size_t Row(SoundId id) {
        return static_cast<std::size_t>(id) - SoundTables::FirstGeneratedId;
    }
    std::set<std::size_t> _playing;
};

// A Ui as a real game has one: the listener set, the sounds among its looks, the device faked.
struct Bench {
    explicit Bench(const UiSounds& sounds = AllSounds)
        : tables(SfxTable)
        , player(tables, output)
        , listener(player)
        , ui(UiSettings{.canvas = {0.0f, 0.0f, 800.0f, 600.0f}, .font = &TestFont()}) {
        ui.Looks().Set(sounds);
        SetActiveUiListener(&listener);
    }
    ~Bench() { SetActiveUiListener(nullptr); }
    Bench(const Bench&) = delete;
    Bench& operator=(const Bench&) = delete;

    using Build = std::function<void(Ui&)>;
    void Run(const Build& build, const DeviceInput& input = {}) {
        ui.Input(input);
        ui.Begin(Frame);
        build(ui);
        ui.End();
    }
    // Shows the screen, then forgets what that played.
    void Settle(const Build& build) {
        for (int frame = 0; frame < 3; ++frame) Run(build);
        Clear();
    }
    void Clear() {
        output.heard.clear();
        output.Finish();
    }

    SoundTables tables;
    FakeOutput output;
    SoundPlayer player;
    UiSoundListener listener;
    Ui ui;
};
using Heard = std::vector<Sfx>;

[[nodiscard]] DeviceInput Pressed(InputKey key) {
    DeviceInput input;
    input.keysPressed[static_cast<std::size_t>(key)] = true;
    input.keysDown[static_cast<std::size_t>(key)] = true;
    return input;
}
[[nodiscard]] DeviceInput Down(InputKey key) {
    DeviceInput input;
    input.keysDown[static_cast<std::size_t>(key)] = true;
    return input;
}
[[nodiscard]] DeviceInput MouseAt(Float2 point) {
    DeviceInput input;
    input.mousePosition = Vec2{static_cast<int>(point.x), static_cast<int>(point.y)};
    return input;
}
[[nodiscard]] DeviceInput MouseMovedTo(Float2 point) {
    DeviceInput input = MouseAt(point);
    input.mouseHover = input.mousePosition;
    return input;
}
[[nodiscard]] DeviceInput MousePressed(Float2 point) {
    DeviceInput input = MouseAt(point);
    input.mousePress = input.mousePosition;
    input.mousePressed = true;
    input.mouseDown = true;
    return input;
}
[[nodiscard]] DeviceInput MouseHeld(Float2 point) {
    DeviceInput input = MouseAt(point);
    input.mouseDown = true;
    return input;
}
[[nodiscard]] DeviceInput MouseReleased(Float2 point) {
    DeviceInput input = MouseAt(point);
    input.mouseReleased = true;
    return input;
}

void Column(Ui& ui, const std::function<void()>& rows) {
    auto column = ui.Element("column", {.width = UiFixed(600.0f), .direction = UiDirection::Column, .gap = 8.0f});
    rows();
}
// Two plain buttons with a custom way to look at the second.
const Bench::Build TwoButtons = [](Ui& ui) {
    Column(ui, [&] {
        static_cast<void>(UiButton(ui, "a", "A", {.width = UiFill()}));
        static_cast<void>(UiButton(ui, "b", "B", {.width = UiFill()}));
    });
};
} // namespace

TEST_CASE("Engine.UiSound.EachEventPlaysWhatTheTableSays", "[engine][ui][sound]") {
    Bench bench;
    bool dialog = false;
    bool flag = false;
    const auto build = [&](Ui& ui) {
        Column(ui, [&] {
            static_cast<void>(UiButton(ui, "first", "First", {.width = UiFill()}));
            static_cast<void>(UiToggle(ui, "flag", "Flag", flag));
            static_cast<void>(UiButton(ui, "off", "Off", {.enabled = false, .width = UiFill()}));
        });
        if (auto layer = ui.Layer("dialog", dialog, {.seconds = 0.0f})) {
            if (ui.Back()) dialog = false;
            static_cast<void>(UiButton(ui, "ok", "OK"));
        }
    };
    bench.Settle(build);
    CHECK(bench.output.heard.empty()); // A cursor placed by the screen plays nothing

    bench.Run(build, Pressed(InputKey::Enter));
    CHECK(bench.output.heard == Heard{Sfx::Confirm});
    bench.Clear();

    bench.Run(build, Pressed(InputKey::Down));
    CHECK(bench.output.heard == Heard{Sfx::Move});
    bench.Clear();

    bench.Run(build, Pressed(InputKey::Right));
    CHECK(flag);
    CHECK(bench.output.heard == Heard{Sfx::Step});
    bench.Clear();

    bench.Run(build, Pressed(InputKey::Enter)); // A press that changes a toggle plays the change, not both
    CHECK_FALSE(flag);
    CHECK(bench.output.heard == Heard{Sfx::Step});
    bench.Clear();

    bench.Run(build, Pressed(InputKey::Down));
    bench.Clear();
    bench.Run(build, Pressed(InputKey::Enter));
    CHECK(bench.output.heard == Heard{Sfx::Deny});
    bench.Clear();

    dialog = true;
    bench.Run(build);
    CHECK(bench.output.heard == Heard{Sfx::Open});
    bench.Settle(build);

    bench.Run(build, Pressed(InputKey::Escape));
    CHECK_FALSE(dialog);
    CHECK(bench.output.heard == Heard{Sfx::Back});
    bench.Clear();

    bench.Run(build);
    CHECK(bench.output.heard == Heard{Sfx::Close});
}

TEST_CASE("Engine.UiSound.PressBackDenyPlayEveryTime", "[engine][ui][sound]") {
    Bench bench;
    const auto build = [&](Ui& ui) {
        Column(ui, [&] {
            static_cast<void>(UiButton(ui, "yes", "Yes", {.width = UiFill()}));
            static_cast<void>(UiButton(ui, "no", "No", {.enabled = false, .width = UiFill()}));
        });
        static_cast<void>(ui.Back());
    };
    bench.Settle(build);
    bench.Run(build, Pressed(InputKey::Enter));
    bench.Run(build, Pressed(InputKey::Enter)); // Nothing marks the first as finished: these overlap, as they should
    CHECK(bench.output.heard == Heard{Sfx::Confirm, Sfx::Confirm});
    bench.Clear();
    bench.Run(build, Pressed(InputKey::Escape));
    bench.Run(build, Pressed(InputKey::Escape));
    CHECK(bench.output.heard == Heard{Sfx::Back, Sfx::Back});
    bench.Clear();
    bench.Run(build, Pressed(InputKey::Down));
    bench.Clear();
    bench.Run(build, Pressed(InputKey::Enter));
    bench.Run(build, Pressed(InputKey::Enter));
    CHECK(bench.output.heard == Heard{Sfx::Deny, Sfx::Deny});
}

TEST_CASE("Engine.UiSound.NothingPlaysWithNoSoundsGiven", "[engine][ui][sound]") {
    Bench bench(UiSounds{});
    int value = 0;
    bool dialog = false;
    const auto build = [&](Ui& ui) {
        Column(ui, [&] {
            static_cast<void>(UiButton(ui, "a", "A", {.width = UiFill()}));
            static_cast<void>(UiSlider(ui, "s", "S", value, 0, 10));
        });
        if (auto layer = UiLayer(ui, "dialog", dialog)) static_cast<void>(ui.Back());
    };
    bench.Settle(build);
    bench.Run(build, Pressed(InputKey::Enter));
    bench.Run(build, Pressed(InputKey::Down));
    bench.Run(build, Pressed(InputKey::Right));
    dialog = true;
    bench.Run(build);
    bench.Run(build, Pressed(InputKey::Escape));
    CHECK(value != 0);
    CHECK(bench.output.heard.empty());
}

TEST_CASE("Engine.UiSound.WithNoListenerAUiWorksAsBefore", "[engine][ui][sound]") {
    Ui ui(UiSettings{.canvas = {0.0f, 0.0f, 800.0f, 600.0f}, .font = &TestFont()});
    ui.Looks().Set(AllSounds);
    ui.Begin(Frame);
    TwoButtons(ui);
    ui.End();
    CHECK(ui.Events().empty());
}

TEST_CASE("Engine.UiSound.AHeldKeyDoesNotOverlapItself", "[engine][ui][sound]") {
    Bench bench;
    int value = 5;
    const auto build = [&](Ui& ui) {
        Column(ui, [&] {
            static_cast<void>(UiSlider(ui, "s", "S", value, 0, 100, 1));
            static_cast<void>(UiButton(ui, "a", "A", {.width = UiFill()}));
            static_cast<void>(UiButton(ui, "b", "B", {.width = UiFill()}));
        });
    };
    bench.ui.Looks().Set(UiSounds{.move = Sfx::Move, .step = Sfx::Step});
    bench.Settle(build);

    SECTION("A slider's repeats") {
        bench.Run(build, Pressed(InputKey::Right));
        for (int frame = 0; frame < 120; ++frame) bench.Run(build, Down(InputKey::Right));
        CHECK(value > 10); // It repeated many times
        CHECK(bench.output.heard == Heard{Sfx::Step});
        bench.output.Finish();
        bench.Run(build, Pressed(InputKey::Right)); // Once it has ended, the next one plays
        CHECK(bench.output.heard == Heard{Sfx::Step, Sfx::Step});
    }
    SECTION("A cursor's repeats") {
        bench.Run(build, Pressed(InputKey::Down));
        for (int frame = 0; frame < 120; ++frame) bench.Run(build, Down(InputKey::Down));
        CHECK(bench.output.heard == Heard{Sfx::Move});
    }
    SECTION("A step falls back on move, under the same rule") {
        bench.ui.Looks().Set(UiSounds{.move = Sfx::Move});
        bench.Run(build, Pressed(InputKey::Right));
        for (int frame = 0; frame < 30; ++frame) bench.Run(build, Down(InputKey::Right));
        CHECK(bench.output.heard == Heard{Sfx::Move});
    }
}

TEST_CASE("Engine.UiSound.TheMouseMovingOntoAnElementPlaysMove", "[engine][ui][sound]") {
    Bench bench;
    FloatRect second{};
    const auto build = [&](Ui& ui) {
        auto column = ui.Element("column", {.width = UiFixed(600.0f), .direction = UiDirection::Column, .gap = 8.0f});
        static_cast<void>(UiButton(ui, "a", "A", {.width = UiFill()}));
        UiElement probe = ui.Element("probe", {.width = UiFill(), .height = UiFixed(40.0f)},
                                     {.interact = UiInteract::Focus});
        second = probe.LastRect();
    };
    bench.Settle(build);
    const Float2 point = second.Center();

    bench.Run(build, MouseAt(point)); // Resting, or not eligible: the cursor stays
    CHECK(bench.output.heard.empty());
    bench.Run(build, MouseMovedTo(point));
    CHECK(bench.output.heard == Heard{Sfx::Move});
    bench.Clear();
    bench.Run(build, MouseAt(point));
    bench.Run(build, MouseMovedTo({point.x + 5.0f, point.y})); // Within the same element: nothing new
    CHECK(bench.output.heard.empty());
}

TEST_CASE("Engine.UiSound.EscapeWithNobodyAskingIsSilent", "[engine][ui][sound]") {
    Bench bench;
    bench.Settle(TwoButtons);
    bench.Run(TwoButtons, Pressed(InputKey::Escape));
    CHECK(bench.output.heard.empty());
    CHECK(bench.ui.Events().empty());
}

TEST_CASE("Engine.UiSound.EscapeInADialogSoundsOnce", "[engine][ui][sound]") {
    Bench bench(UiSounds{.back = Sfx::Back});
    bool dialog = true;
    int menuBacks = 0;
    const auto build = [&](Ui& ui) {
        Column(ui, [&] { static_cast<void>(UiButton(ui, "a", "A", {.width = UiFill()})); });
        if (ui.Back()) ++menuBacks; // The menu asks too, every frame, and is below the dialog
        if (auto layer = ui.Layer("dialog", dialog, {.seconds = 0.0f})) {
            static_cast<void>(UiButton(ui, "stay", "Stay"));
            if (ui.Back()) dialog = false;
            if (ui.Back()) dialog = false; // Asking twice is one back
        }
    };
    bench.Settle(build);
    bench.Run(build, Pressed(InputKey::Escape));
    CHECK_FALSE(dialog);
    CHECK(menuBacks == 0);
    CHECK(bench.output.heard == Heard{Sfx::Back});
    int backEvents = 0;
    for (const UiEvent& event : bench.ui.Events()) backEvents += event.kind == UiEventKind::Back ? 1 : 0;
    CHECK(backEvents == 1); // For a game reading the events too
    bench.Clear();
    bench.Run(build);
    CHECK(bench.output.heard.empty()); // No close sound was given
}

TEST_CASE("Engine.UiSound.ABackButtonAndABackKeyInOneFrameSoundOnce", "[engine][ui][sound]") {
    Bench bench;
    const auto build = [&](Ui& ui) {
        static_cast<void>(UiButton(ui, "back", "Back", {.sound = UiSound::Back}));
        static_cast<void>(ui.Back()); // Asked either way, not after the button as with ||
    };
    bench.Settle(build);
    DeviceInput both = Pressed(InputKey::Enter);
    both.keysPressed[static_cast<std::size_t>(InputKey::Escape)] = true;
    bench.Run(build, both);
    CHECK(bench.output.heard == Heard{Sfx::Back});
}

TEST_CASE("Engine.UiSound.TwoPlayersPressingBackInOneFrameSoundOnce", "[engine][ui][sound]") {
    Bench bench;
    bench.ui.SetUsers({UiKeys::Wasd(), UiKeys::Arrows()});
    const auto build = [&](Ui& ui) { static_cast<void>(ui.Back()); };
    bench.Settle(build);
    DeviceInput both;
    for (const InputKey key : UiKeys::Wasd().back) both.keysPressed[static_cast<std::size_t>(key)] = true;
    for (const InputKey key : UiKeys::Arrows().back) both.keysPressed[static_cast<std::size_t>(key)] = true;
    bench.Run(build, both);
    CHECK(bench.output.heard == Heard{Sfx::Back});
}

TEST_CASE("Engine.UiSound.OneElementCanDiffer", "[engine][ui][sound]") {
    Bench bench;
    UiSound sound = UiSound::Auto;
    bool flag = false;
    const auto build = [&](Ui& ui) {
        Column(ui, [&] {
            static_cast<void>(UiButton(ui, "button", "Button", {.width = UiFill(), .sound = sound}));
            static_cast<void>(UiToggle(ui, "toggle", "Toggle", flag, {.sound = sound}));
        });
    };
    bench.Settle(build);

    SECTION("A press plays what the button says") {
        sound = UiSound::Back;
        bench.Run(build);
        bench.Run(build, Pressed(InputKey::Enter));
        CHECK(bench.output.heard == Heard{Sfx::Back});
    }
    SECTION("None: the game plays its own, and the event is still there") {
        sound = UiSound::None;
        bench.Run(build);
        bench.Run(build, Pressed(InputKey::Enter));
        CHECK(bench.output.heard.empty());
        REQUIRE(bench.ui.Events().size() == 1);
        CHECK(bench.ui.Events()[0].kind == UiEventKind::Activated);
    }
    SECTION("On a control it is what a change plays") {
        sound = UiSound::Confirm;
        bench.Run(build);
        bench.Run(build, Pressed(InputKey::Down));
        bench.Clear();
        bench.Run(build, Pressed(InputKey::Right));
        CHECK(flag);
        CHECK(bench.output.heard == Heard{Sfx::Confirm});
    }
    SECTION("It does not change the cursor's sound") {
        sound = UiSound::None;
        bench.Run(build);
        bench.Run(build, Pressed(InputKey::Down));
        CHECK(bench.output.heard == Heard{Sfx::Move});
    }
}

TEST_CASE("Engine.UiSound.AGamesOwnWidgetChoosesThroughItsTraits", "[engine][ui][sound]") {
    Bench bench;
    int value = 0;
    const auto build = [&](Ui& ui) {
        UiElement own = ui.Element("own", {.width = UiFixed(60.0f), .height = UiFixed(60.0f)},
                                   {.interact = UiInteract::Focus,
                                    .capture = UiCapture::Horizontal,
                                    .sound = UiSound::Open});
        if (const UiStep step = own.Step(); step.x > 0) {
            ++value;
            own.ReportChange(step.user, UiSound::Close);
        }
    };
    bench.Settle(build);
    bench.Run(build, Pressed(InputKey::Enter));
    CHECK(bench.output.heard == Heard{Sfx::Open});
    bench.Clear();
    bench.Run(build, Pressed(InputKey::Right));
    CHECK(value == 1);
    CHECK(bench.output.heard == Heard{Sfx::Close});
    bench.Clear();
    bench.Run(build, Pressed(InputKey::Left)); // A key it takes and ignores says nothing
    CHECK(bench.output.heard.empty());
}

TEST_CASE("Engine.UiSound.ASliderSoundsWhenItChanges", "[engine][ui][sound]") {
    Bench bench;
    int value = 50;
    FloatRect row{};
    const auto build = [&](Ui& ui) {
        Column(ui, [&] {
            static_cast<void>(UiSlider(ui, "s", "Level", value, 0, 100, 10));
            UiElement probe = ui.Element("probe", {.width = UiFill(), .height = UiFixed(1.0f)});
            row = probe.LastRect();
        });
    };
    bench.Settle(build);
    // The track lies in the right half of the row, above the probe.
    const float y = row.y - 12.0f;
    const float left = row.x + row.width * 0.6f;
    const float right = row.x + row.width - 1.0f;

    SECTION("Dragged, it sounds at each change and not when the value stays") {
        bench.Run(build, MousePressed({left, y}));
        const int atPress = value;
        const auto first = bench.output.heard.size();
        CHECK(first == (atPress != 50 ? 1U : 0U));
        bench.Clear();
        bench.Run(build, MouseHeld({left, y})); // Held still
        CHECK(bench.output.heard.empty());
        bench.Run(build, MouseHeld({right, y}));
        CHECK(value == 100);
        CHECK(bench.output.heard == Heard{Sfx::Step});
        bench.output.Finish();
        bench.Run(build, MouseHeld({right + 40.0f, y})); // Past its end: nothing changes
        CHECK(bench.output.heard == Heard{Sfx::Step});
        bench.Clear();
        bench.Run(build, MouseReleased({right + 40.0f, y})); // Letting go is not a press
        CHECK(bench.output.heard.empty());
    }
    SECTION("A click on the row that changes nothing is silent") {
        value = 100;
        bench.Run(build, MousePressed({right, y}));
        bench.Run(build, MouseReleased({right, y}));
        CHECK(value == 100);
        CHECK(bench.output.heard.empty());
    }
    SECTION("At its end, keys are silent") {
        value = 100;
        bench.Run(build, Pressed(InputKey::Right));
        CHECK(bench.output.heard.empty());
        bench.Run(build, Pressed(InputKey::Left));
        CHECK(value == 90);
        CHECK(bench.output.heard == Heard{Sfx::Step});
    }
    SECTION("Confirm on it does nothing, and says nothing") {
        bench.Run(build, Pressed(InputKey::Enter));
        CHECK(bench.output.heard.empty());
    }
}

TEST_CASE("Engine.UiSound.AToggleAlreadyOnIsSilent", "[engine][ui][sound]") {
    Bench bench;
    bool flag = true;
    const auto build = [&](Ui& ui) {
        static_cast<void>(UiToggle(ui, "t", "Toggle", flag, {.width = UiFixed(400.0f)}));
    };
    bench.Settle(build);
    bench.Run(build, Pressed(InputKey::Right));
    CHECK(flag);
    CHECK(bench.output.heard.empty());
    bench.Run(build, Pressed(InputKey::Left));
    CHECK_FALSE(flag);
    CHECK(bench.output.heard == Heard{Sfx::Step});
}

TEST_CASE("Engine.UiSound.ASteppersListOfOneStepsNowhere", "[engine][ui][sound]") {
    Bench bench;
    int chosen = 0;
    const std::array<std::string_view, 1> one{"Only"};
    const std::array<std::string_view, 2> two{"A", "B"};
    bool single = true;
    const auto build = [&](Ui& ui) {
        const std::span<const std::string_view> list =
            single ? std::span<const std::string_view>(one) : std::span<const std::string_view>(two);
        static_cast<void>(UiStepper(ui, "s", "Stepper", chosen, list, {.width = UiFixed(400.0f)}));
    };
    bench.Settle(build);
    bench.Run(build, Pressed(InputKey::Right));
    bench.Run(build, Pressed(InputKey::Enter));
    CHECK(bench.output.heard.empty());
    single = false;
    bench.Run(build);
    bench.Run(build, Pressed(InputKey::Right));
    CHECK(chosen == 1);
    CHECK(bench.output.heard == Heard{Sfx::Step});
    bench.Clear();
    bench.Run(build, Pressed(InputKey::Enter)); // Confirm steps forward; one sound, not a press and a step
    CHECK(chosen == 0);
    CHECK(bench.output.heard == Heard{Sfx::Step});
}

TEST_CASE("Engine.UiSound.ASwitchOfTabsIsAChange", "[engine][ui][sound]") {
    Bench bench;
    int page = 0;
    const std::array<std::string_view, 2> names{"One", "Two"};
    FloatRect firstTab{};
    const auto build = [&](Ui& ui) {
        static_cast<void>(UiTabs(ui, "tabs", names, page));
        UiElement probe = ui.Element("probe", {.width = UiFixed(1.0f), .height = UiFixed(1.0f)});
        firstTab = probe.LastRect();
    };
    bench.Settle(build);
    bench.Run(build, Pressed(InputKey::Right));
    CHECK(page == 1);
    CHECK(bench.output.heard == Heard{Sfx::Step});
    bench.Clear();
    bench.Run(build, Pressed(InputKey::Enter)); // A press on the row does nothing
    CHECK(bench.output.heard.empty());
    const Float2 onFirst{firstTab.x + 10.0f, firstTab.y - 10.0f};
    bench.Run(build, MousePressed(onFirst));
    bench.Run(build, MouseReleased(onFirst));
    CHECK(page == 0);
    CHECK(bench.output.heard == Heard{Sfx::Step});
    bench.Clear();
    bench.Run(build, MousePressed(onFirst)); // Already open
    bench.Run(build, MouseReleased(onFirst));
    CHECK(bench.output.heard.empty());
}

TEST_CASE("Engine.UiSound.TheScrollBarIsNotAPress", "[engine][ui][sound]") {
    Bench bench;
    const auto build = [&](Ui& ui) {
        auto panel = ui.Element("panel", {.width = UiFixed(300.0f), .height = UiFixed(100.0f)});
        UiScroll list(ui, "list", {.gap = 4.0f});
        for (int item = 0; item < 20; ++item) static_cast<void>(UiButton(ui, item, "Item", {.width = UiFill()}));
    };
    bench.Settle(build);
    const Float2 point{296.0f, 50.0f}; // The bar lies at the right edge of the panel
    bench.Run(build, MousePressed(point));
    bench.Run(build, MouseReleased(point));
    CHECK(bench.output.heard.empty());
}

TEST_CASE("Engine.UiSound.ADisabledElementDenies", "[engine][ui][sound]") {
    Bench bench;
    int value = 5;
    bool flag = false;
    const auto build = [&](Ui& ui) {
        Column(ui, [&] {
            static_cast<void>(UiSlider(ui, "s", "S", value, 0, 10, 1, {.enabled = false}));
            static_cast<void>(UiToggle(ui, "t", "T", flag, {.enabled = false}));
            static_cast<void>(UiButton(ui, "b", "B", {.enabled = false, .width = UiFill(), .sound = UiSound::None}));
        });
    };
    bench.Settle(build);
    bench.Run(build, Pressed(InputKey::Right)); // A step on a slider
    CHECK(value == 5);
    CHECK(bench.output.heard == Heard{Sfx::Deny});
    bench.Clear();
    bench.Run(build, Pressed(InputKey::Enter)); // A press on it
    CHECK(bench.output.heard == Heard{Sfx::Deny});
    bench.Clear();
    bench.Run(build, Pressed(InputKey::Down));
    bench.Clear();
    bench.Run(build, Pressed(InputKey::Left)); // Off already, but disabled refuses whatever the key
    CHECK(bench.output.heard == Heard{Sfx::Deny});
    bench.Clear();
    bench.Run(build, Pressed(InputKey::Down));
    bench.Clear();
    bench.Run(build, Pressed(InputKey::Enter)); // `.sound` is about a press that happens, and this one is refused
    CHECK(bench.output.heard == Heard{Sfx::Deny});
}

TEST_CASE("Engine.UiSound.ALayerSoundsOnlyIfItsSoundsAreGiven", "[engine][ui][sound]") {
    Bench bench(UiSounds{.move = Sfx::Move});
    bool dialog = false;
    bool notice = false;
    const auto build = [&](Ui& ui) {
        if (auto layer = ui.Layer("dialog", dialog, {.seconds = 0.0f})) {}
        if (auto layer = ui.Layer("notice", notice, {.seconds = 0.0f, .modal = false})) {}
    };
    bench.Settle(build);
    dialog = true;
    bench.Run(build);
    dialog = false;
    bench.Run(build);
    CHECK(bench.output.heard.empty());

    bench.ui.Looks().Set(AllSounds);
    notice = true;
    bench.Run(build);
    notice = false;
    bench.Run(build);
    CHECK(bench.output.heard.empty()); // A notice coming and going says nothing
}

TEST_CASE("Engine.UiSound.AMissingSoundFailsAtStartup", "[engine][ui][sound]") {
    const SoundTables tables(SfxTable);
    FakeOutput output;
    const SoundPlayer player(tables, output);
    CHECK_NOTHROW(CheckUiSounds({}, player, "GameSettings::uiSounds"));
    CHECK_NOTHROW(CheckUiSounds(AllSounds, player, "GameSettings::uiSounds"));

    SECTION("An enum whose table was never registered") {
        REQUIRE_THROWS_AS(CheckUiSounds({.confirm = Stranger::Nobody}, player, "GameSettings::uiSounds"),
                          std::invalid_argument);
        REQUIRE_THROWS_WITH(CheckUiSounds({.confirm = Stranger::Nobody}, player, "GameSettings::uiSounds"),
                            Catch::Matchers::ContainsSubstring("GameSettings::uiSounds.confirm") &&
                                Catch::Matchers::ContainsSubstring("Stranger"));
    }
    SECTION("Count is no row") {
        REQUIRE_THROWS_WITH(CheckUiSounds({.deny = Sfx::Count}, player, "GameSettings::uiSounds"),
                            Catch::Matchers::ContainsSubstring("GameSettings::uiSounds.deny"));
    }
}

TEST_CASE("Engine.UiSound.AUisOwnBadSoundFailsOnItsFirstFrame", "[engine][ui][sound]") {
    Bench bench;
    bench.ui.Looks().Set(UiSounds{.close = Stranger::Nobody});
    bench.ui.Begin(Frame);
    REQUIRE_THROWS_WITH(bench.ui.End(), Catch::Matchers::ContainsSubstring("close"));
}

TEST_CASE("Engine.UiSound.AUiWithItsOwnSoundsOverridesTheGames", "[engine][ui][sound]") {
    const UiLooks saved = ActiveUiLooks();
    UiLooks game;
    game.Set(UiSounds{.confirm = Sfx::Confirm});
    SetActiveUiLooks(game); // What Game does with GameSettings::uiSounds

    {
        const SoundTables tables(SfxTable);
        FakeOutput output;
        SoundPlayer player(tables, output);
        UiSoundListener listener(player);
        SetActiveUiListener(&listener);
        const UiSettings settings{.canvas = {0.0f, 0.0f, 800.0f, 600.0f}, .font = &TestFont()};
        Ui plain(settings);
        Ui shop(settings);
        shop.Looks().Set(UiSounds{.confirm = Sfx::Other});
        const auto press = [&](Ui& ui) {
            for (int frame = 0; frame < 3; ++frame) {
                ui.Input(frame == 2 ? Pressed(InputKey::Enter) : DeviceInput{});
                ui.Begin(Frame);
                TwoButtons(ui);
                ui.End();
            }
        };
        press(plain);
        CHECK(output.heard == Heard{Sfx::Confirm});
        output.heard.clear();
        press(shop);
        CHECK(output.heard == Heard{Sfx::Other});
        SetActiveUiListener(nullptr);
    }
    SetActiveUiLooks(saved);
}

TEST_CASE("Engine.UiSound.TwoPlayersMovingInOneFramePlayOneMove", "[engine][ui][sound]") {
    Bench bench;
    bench.ui.SetUsers({UiKeys::Wasd(), UiKeys::Arrows()});
    const auto build = [&](Ui& ui) {
        Column(ui, [&] {
            static_cast<void>(UiButton(ui, "a", "A", {.width = UiFill()}));
            static_cast<void>(UiButton(ui, "b", "B", {.width = UiFill()}));
        });
    };
    bench.Settle(build);
    DeviceInput both;
    for (const InputKey key : UiKeys::Wasd().down) both.keysPressed[static_cast<std::size_t>(key)] = true;
    for (const InputKey key : UiKeys::Arrows().down) both.keysPressed[static_cast<std::size_t>(key)] = true;
    bench.Run(build, both);
    int moves = 0;
    for (const UiEvent& event : bench.ui.Events()) moves += event.kind == UiEventKind::FocusMoved ? 1 : 0;
    CHECK(moves == 2); // Both cursors moved; the game can still see that
    CHECK(bench.output.heard == Heard{Sfx::Move});
}

TEST_CASE("Engine.UiSound.TheOptionsBackButtonAndEscapeSoundTheSame", "[engine][ui][sound]") {
    Bench bench;
    UiOptionsValues values;
    bool closed = false;
    FloatRect screen{};
    const auto build = [&](Ui& ui) {
        closed = UiOptionsScreen(ui, "options", values).closed;
        screen = ui.Canvas();
    };
    bench.Settle(build);

    SECTION("Escape") {
        bench.Run(build, Pressed(InputKey::Escape));
        CHECK(closed);
        CHECK(bench.output.heard == Heard{Sfx::Back});
    }
    SECTION("The button, by the keyboard, down past the two sliders") {
        for (int move = 0; move < 3; ++move) bench.Run(build, Pressed(InputKey::Down));
        bench.Clear();
        bench.Run(build, Pressed(InputKey::Enter));
        CHECK(closed);
        CHECK(bench.output.heard == Heard{Sfx::Back});
    }
    SECTION("The effects slider sounds when it changes") {
        for (int move = 0; move < 2; ++move) bench.Run(build, Pressed(InputKey::Down));
        bench.Clear();
        const int before = values.effectsPercent;
        bench.Run(build, Pressed(values.effectsPercent >= 100 ? InputKey::Left : InputKey::Right));
        CHECK(values.effectsPercent != before);
        CHECK(bench.output.heard == Heard{Sfx::Step});
    }
}

TEST_CASE("Engine.UiSound.TheMeaningOfEachEvent", "[engine][ui][sound]") {
    const auto of = [](UiEventKind kind, UiSound sound = UiSound::Auto) {
        return UiSoundOf(UiEvent{.kind = kind, .sound = sound});
    };
    CHECK(of(UiEventKind::FocusMoved) == UiSound::Move);
    CHECK(of(UiEventKind::Activated) == UiSound::Confirm);
    CHECK(of(UiEventKind::Changed) == UiSound::Step);
    CHECK(of(UiEventKind::Denied) == UiSound::Deny);
    CHECK(of(UiEventKind::Back) == UiSound::Back);
    CHECK(of(UiEventKind::LayerOpened) == UiSound::Open);
    CHECK(of(UiEventKind::LayerClosed) == UiSound::Close);
    CHECK(of(UiEventKind::Activated, UiSound::None) == UiSound::None);
    CHECK_FALSE(UiSoundKey(AllSounds, UiSound::None));
    CHECK_FALSE(UiSoundKey(UiSounds{}, UiSound::Step));
    CHECK(UiSoundKey(UiSounds{.move = Sfx::Move}, UiSound::Step).Index() == static_cast<std::size_t>(Sfx::Move));
    CHECK(UiSoundKey(AllSounds, UiSound::Step).Index() == static_cast<std::size_t>(Sfx::Step));
}
