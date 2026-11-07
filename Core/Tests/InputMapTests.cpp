#include "Tests/Support/Diagnostics.h"
#include <filesystem>
#include <string>
#include "Input/InputBindings.h"
#include "Input/InputMap.h"
#include "Platform/Internals/FileStorage.h"
#include "Simulation/SimulationSettings.h"
#include "TestPad.input.h"
#include "Tuning/Internals/TuningJson.h"

TEST_CASE("Engine.InputMap.SharedCasesMatchTheGenerator", "[engine][input-map]") {
    const std::filesystem::path cases(ENGINE_INPUT_MAP_CASES_DIR);
    const auto list = ParseTuningJson(ReadBinaryFile(cases / "cases.json"));
    const auto* entries = list.Find("cases");
    REQUIRE(entries);
    REQUIRE_FALSE(entries->items.empty());
    for (const auto& entry : entries->items) {
        const std::string file = entry.Find("file")->text;
        const auto* line = entry.Find("line");
        INFO(file);
        const std::string text = ReadBinaryFile(cases / file);
        if (line->kind == TuningJsonKind::Null) {
            CHECK_NOTHROW((void)ParseInputMap(text));
            continue;
        }
        try {
            (void)ParseInputMap(text);
            FAIL("expected an error on line " << line->text);
        } catch (const TuningFormatError& error) {
            INFO(error.what());
            CHECK(std::to_string(error.Line()) == line->text);
        }
    }
}

TEST_CASE("Engine.InputMap.KeyNamesRoundTrip", "[engine][input-map]") {
    for (int i = 0; i < static_cast<int>(InputKey::Count); ++i) {
        const auto key = static_cast<InputKey>(i);
        CHECK(FindInputKey(InputKeyName(key)) == key);
    }
    CHECK(InputKeyName(InputKey::LeftShift) == "LeftShift");
    CHECK_FALSE(FindInputKey("Spacebar"));
}

TEST_CASE("Engine.InputMap.BindsAnActionToMouseRight", "[engine][input-map]") {
    const auto map =
        ParseInputMap(R"({"buttons": ["Fire", "Aim"], "players": [{"Fire": ["MouseLeft"], "Aim": ["MouseRight"]}]})");
    REQUIRE(map.players[0][1] == std::vector<std::string>{"MouseRight"});
    CHECK(InputKeyName(InputKey::MouseMiddle) == "MouseMiddle");
    InputBindings bindings(1);
    for (std::size_t button = 0; button < map.buttons.size(); ++button)
        for (const auto& name : *map.players[0][button])
            bindings.Bind(std::span<const InputMapBinding>({InputMapBinding{0, button, *FindInputKey(name)}}));
    DeviceInput click;
    click.keysPressed[static_cast<std::size_t>(InputKey::MouseRight)] = true;
    click.keysDown[static_cast<std::size_t>(InputKey::MouseRight)] = true;
    auto players = bindings.Read(click);
    CHECK(players[0].Pressed(1));
    CHECK(players[0].Held(1));
    CHECK_FALSE(players[0].Held(0));
    DeviceInput held;
    held.keysDown[static_cast<std::size_t>(InputKey::MouseRight)] = true;
    players = bindings.Read(held);
    CHECK_FALSE(players[0].Pressed(1));
    CHECK(players[0].Held(1));
    CHECK_FALSE(bindings.Read(DeviceInput{})[0].Held(1));
}

TEST_CASE("Engine.InputMap.PatchReplacesOnePlayersKeys", "[engine][input-map]") {
    const std::string text = "{\"buttons\": [\"Left\", \"Jump\"], \"players\": [{\"Jump\": [\"Space\"]}, {}]}";
    auto patched = PatchInputMapKeys(text, 1, "Left", {"Left", "A"});
    CHECK(patched == "{\n  \"buttons\": [\"Left\", \"Jump\"],\n  \"axes\": [],\n  \"players\": [\n"
                     "    { \"Jump\": [\"Space\"] },\n    { \"Left\": [\"Left\", \"A\"] }\n  ]\n}\n");
    patched = PatchInputMapKeys(patched, 0, "Jump", {});
    CHECK(ParseInputMap(patched).players[0][1] == std::nullopt);
    CHECK_THROWS_AS(PatchInputMapKeys(text, 2, "Left", {"A"}), TuningFormatError);
    CHECK_THROWS_AS(PatchInputMapKeys(text, 0, "Fire", {"A"}), TuningFormatError);
    CHECK_THROWS_AS(PatchInputMapKeys(text, 0, "Left", {"Spacebar"}), TuningFormatError);
    CHECK_THROWS_AS(PatchInputMapKeys(text, 0, "Left", {"A", "A"}), TuningFormatError);
}

TEST_CASE("Engine.InputMap.GeneratedHeaderBindsItsKeys", "[engine][input-map]") {
    STATIC_REQUIRE(TestPadInput::ButtonNames[1] == "Jump");
    STATIC_REQUIRE(TestPadInput::Players == 2);
    STATIC_REQUIRE(static_cast<int>(TestPadInput::Axis::MoveX) == 0);
    InputBindings bindings(2);
    bindings.BindMap<TestPadInput>();
    DeviceInput input;
    input.keysDown[static_cast<std::size_t>(InputKey::Left)] = true;
    input.keysDown[static_cast<std::size_t>(InputKey::RightControl)] = true;
    const auto players = bindings.Read(input);
    CHECK(players[0].Held(TestPadInput::Button::Left));
    CHECK_FALSE(players[0].Held(TestPadInput::Button::Jump));
    CHECK(players[1].Held(TestPadInput::Button::Jump));

    InputBindings single(1); // The map's second player is left out
    single.BindMap<TestPadInput>();
    CHECK_FALSE(single.Read(input)[0].Held(TestPadInput::Button::Jump));
}

TEST_CASE("Engine.InputMap.DeclareInputNamesTheButtons", "[engine][input-map]") {
    SimulationSettings settings;
    DeclareInput<TestPadInput>(settings);
    CHECK(settings.buttons == std::vector<std::string>{"Left", "Jump"});
    CHECK(settings.axes == std::vector<std::string>{"MoveX"});
}

TEST_CASE("Engine.InputMap.ChangedButtonsKeepTheBuiltKeys", "[engine][input-map]") {
    const std::vector<InputMapBinding> compiled{{0, 0, InputKey::A}};
    const std::array<std::string_view, 1> buttons{"Other"};
    CHECK(ReadInputMapBindings(TestPadInput::InputSource, buttons, compiled) == compiled);
    const std::array<std::string_view, 2> same{"Left", "Jump"};
    const auto read = ReadInputMapBindings(TestPadInput::InputSource, same, compiled);
    CHECK(read.size() == 4);
    CHECK(read.back() == InputMapBinding{1, 1, InputKey::RightControl});
}
