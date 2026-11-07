#include "Input/InputMap.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include "Platform/Internals/FileStorage.h"
#include "Tuning/Internals/TuningJson.h"
#include "Tuning/TuningLive.h"

namespace {
constexpr std::size_t MaxButtons = 32; // MaxPlayerButtons
constexpr std::size_t MaxAxes = 8;     // MaxPlayerAxes
constexpr std::size_t MaxPlayers = 16;
constexpr std::size_t MaxFileBytes = 1024 * 1024;
// enum class InputKey, in order; Tools/Tests/test_inputmap.py checks it against the enum.
// clang-format off
constexpr std::array<std::string_view, static_cast<std::size_t>(InputKey::Count)> KeyNames{
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    "Zero", "One", "Two", "Three", "Four", "Five", "Six", "Seven", "Eight", "Nine",
    "Up", "Down", "Left", "Right",
    "Enter", "Space", "Escape",
    "LeftShift", "RightShift", "LeftControl", "RightControl", "LeftAlt", "RightAlt",
    "Tab", "Backspace", "Delete", "Insert",
    "Home", "End", "PageUp", "PageDown",
    "Minus", "Equal", "Subtract", "Add",
    "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    "MouseLeft", "MouseRight", "MouseMiddle"};
// clang-format on

[[noreturn]] void Fail(int line, const std::string& text) { throw TuningFormatError(line, text); }

std::string Quoted(std::string_view text) { return "\"" + std::string(text) + "\""; }

bool Pascal(std::string_view text) {
    return !text.empty() && text[0] >= 'A' && text[0] <= 'Z' && std::all_of(text.begin(), text.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    });
}

std::vector<std::string> Names(const TuningJsonNode& node, std::string_view what, std::size_t limit,
                               std::size_t atLeast) {
    const std::string where = Quoted(what);
    if (node.kind != TuningJsonKind::Array) Fail(node.line, where + " must be a list of names");
    if (node.items.size() < atLeast) Fail(node.line, where + " needs at least " + std::to_string(atLeast) + " name");
    if (node.items.size() > limit) Fail(node.line, where + " has more than " + std::to_string(limit) + " names");
    std::vector<std::string> names;
    for (const auto& item : node.items) {
        if (item.kind != TuningJsonKind::String || !Pascal(item.text))
            Fail(item.line, where + " name " +
                                Quoted(item.kind == TuningJsonKind::String ? item.text : "a non-text value") +
                                " must be PascalCase: an uppercase letter, then letters and digits");
        if (std::ranges::find(names, item.text) != names.end())
            Fail(item.line, where + " lists " + Quoted(item.text) + " twice");
        names.push_back(item.text);
    }
    return names;
}

std::vector<std::optional<std::vector<std::string>>> Player(const TuningJsonNode& node, std::size_t index,
                                                            const std::vector<std::string>& buttons) {
    const std::string where = "player " + std::to_string(index);
    if (node.kind != TuningJsonKind::Object) Fail(node.line, where + " must be an object of button keys");
    std::vector<std::optional<std::vector<std::string>>> keys(buttons.size());
    for (const auto& member : node.members) {
        const auto button = std::ranges::find(buttons, member.key);
        if (button == buttons.end()) Fail(member.line, where + " binds unknown button " + Quoted(member.key));
        const auto& list = member.value;
        const std::string label = where + " " + Quoted(member.key);
        if (list.kind != TuningJsonKind::Array) Fail(list.line, label + " must be a list of key names");
        std::vector<std::string> names;
        for (const auto& item : list.items) {
            if (item.kind != TuningJsonKind::String || !FindInputKey(item.text))
                Fail(item.line, label + ": " +
                                    Quoted(item.kind == TuningJsonKind::String ? item.text : "a non-text value") +
                                    " is not a key name (Input/Input.h InputKey)");
            if (std::ranges::find(names, item.text) != names.end())
                Fail(item.line, label + " lists " + Quoted(item.text) + " twice");
            names.push_back(item.text);
        }
        keys[static_cast<std::size_t>(button - buttons.begin())] = std::move(names);
    }
    return keys;
}

std::string NameList(const std::vector<std::string>& names) {
    std::string text = "[";
    for (std::size_t i = 0; i < names.size(); ++i)
        text += (i ? ", " : "") + Quoted(names[i]);
    return text + "]";
}

std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.generic_u8string();
    return {text.begin(), text.end()};
}
} // namespace

InputMapDocument ParseInputMap(std::string_view text) {
    const auto root = ParseTuningJson(text);
    if (root.kind != TuningJsonKind::Object)
        Fail(root.line, "an input map is a JSON object with buttons, axes and players");
    for (const auto& member : root.members)
        if (member.key != "buttons" && member.key != "axes" && member.key != "players")
            Fail(member.line, "unknown key " + Quoted(member.key) + " (allowed: buttons, axes, players)");
    for (const auto* required : {"buttons", "players"})
        if (!root.Find(required)) Fail(root.line, std::string("an input map needs ") + Quoted(required));
    InputMapDocument map;
    map.buttons = Names(*root.Find("buttons"), "buttons", MaxButtons, 1);
    if (const auto* axes = root.Find("axes")) map.axes = Names(*axes, "axes", MaxAxes, 0);
    const auto& players = *root.Find("players");
    if (players.kind != TuningJsonKind::Array || players.items.empty())
        Fail(players.line, "\"players\" must be a non-empty list");
    if (players.items.size() > MaxPlayers)
        Fail(players.line, "\"players\" has more than " + std::to_string(MaxPlayers) + " players");
    for (std::size_t i = 0; i < players.items.size(); ++i)
        map.players.push_back(Player(players.items[i], i, map.buttons));
    return map;
}

std::string InputMapText(const InputMapDocument& map) {
    std::string text =
        "{\n  \"buttons\": " + NameList(map.buttons) + ",\n  \"axes\": " + NameList(map.axes) + ",\n  \"players\": [\n";
    for (std::size_t p = 0; p < map.players.size(); ++p) {
        std::string cells;
        for (std::size_t b = 0; b < map.buttons.size() && b < map.players[p].size(); ++b)
            if (const auto& keys = map.players[p][b])
                cells += (cells.empty() ? "" : ", ") + Quoted(map.buttons[b]) + ": " + NameList(*keys);
        text += "    {" + (cells.empty() ? std::string() : " " + cells + " ") + "}" +
                (p + 1 < map.players.size() ? ",\n" : "\n");
    }
    return text + "  ]\n}\n";
}

std::string PatchInputMapKeys(std::string_view text, std::size_t player, std::string_view button,
                              const std::vector<std::string>& keys) {
    auto map = ParseInputMap(text);
    if (player >= map.players.size()) Fail(1, "no player " + std::to_string(player));
    const auto found = std::ranges::find(map.buttons, button);
    if (found == map.buttons.end()) Fail(1, "no button " + Quoted(button));
    auto& slot = map.players[player][static_cast<std::size_t>(found - map.buttons.begin())];
    slot.reset();
    if (!keys.empty()) slot = keys;
    auto patched = InputMapText(map);
    (void)ParseInputMap(patched); // An unknown or repeated key is rejected here; nothing is written
    return patched;
}

std::string_view InputKeyName(InputKey key) {
    const auto index = static_cast<std::size_t>(key);
    return index < KeyNames.size() ? KeyNames[index] : std::string_view("?");
}

std::optional<InputKey> FindInputKey(std::string_view name) {
    const auto found = std::ranges::find(KeyNames, name);
    if (found == KeyNames.end()) return std::nullopt;
    return static_cast<InputKey>(found - KeyNames.begin());
}

std::vector<InputMapBinding> ReadInputMapBindings(std::u8string_view source, std::span<const std::string_view> buttons,
                                                  std::vector<InputMapBinding> compiled) {
    const auto file = TuningRoot() / std::filesystem::path(source);
    try {
        const auto map = ParseInputMap(ReadBinaryFile(file, MaxFileBytes));
        if (!std::ranges::equal(map.buttons, buttons)) {
            std::fprintf(stderr, "%s: input map not applied: its buttons changed; build to apply\n",
                         Utf8(file).c_str());
            return compiled;
        }
        std::vector<InputMapBinding> keys;
        for (std::size_t p = 0; p < map.players.size(); ++p)
            for (std::size_t b = 0; b < map.players[p].size(); ++b)
                if (const auto& names = map.players[p][b])
                    for (const auto& name : *names)
                        keys.push_back({static_cast<int>(p), b, *FindInputKey(name)});
        return keys;
    } catch (const TuningFormatError& failure) {
        std::fprintf(stderr, "%s:%d: input map not applied: %s\n", Utf8(file).c_str(), failure.Line(), failure.what());
    } catch (const std::exception& failure) {
        std::fprintf(stderr, "%s: input map unreadable: %s\n", Utf8(file).c_str(), failure.what());
    }
    return compiled;
}
