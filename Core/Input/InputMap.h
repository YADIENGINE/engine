#pragma once
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Input/Input.h"
#include "Tuning/TuningFile.h"

// Input map files (Docs/InputMap.md): `X.input.json` names a game's buttons and axes and each player's default keys,
// and the build turns it into `X.input.h` (struct XInput). Bind its keys with InputBindings::BindMap<XInput>() and
// declare its buttons with DeclareInput<XInput>(SimulationSettings&). The generator (Tools/tuning/inputmap.py) applies
// the same rules; shared cases in Tools/Tests/InputMapCases check both. Errors throw TuningFormatError with the line.
struct InputMapDocument {
    std::vector<std::string> buttons;
    std::vector<std::string> axes;
    // players[player][button]: that button's key names; nullopt when the file leaves the button out.
    std::vector<std::vector<std::optional<std::vector<std::string>>>> players;
};
[[nodiscard]] InputMapDocument ParseInputMap(std::string_view text);
// The canonical layout.
[[nodiscard]] std::string InputMapText(const InputMapDocument& map);
// The file with one player's keys for one button replaced (an empty list leaves the button out), in the canonical
// layout. Throws TuningFormatError for an invalid file, player, button or key name.
[[nodiscard]] std::string PatchInputMapKeys(std::string_view text, std::size_t player, std::string_view button,
                                            const std::vector<std::string>& keys);

// An InputKey's name as an input map writes it: "Space", "LeftShift", "F1".
[[nodiscard]] std::string_view InputKeyName(InputKey key);
[[nodiscard]] std::optional<InputKey> FindInputKey(std::string_view name);

// One key to bind: player, button (its enum value) and key.
struct InputMapBinding {
    int player = 0;
    std::size_t button = 0;
    InputKey key = InputKey::Space;
    friend bool operator==(const InputMapBinding&, const InputMapBinding&) = default;
};
// Development builds: the keys of the input map at `source` (from the project folder) when its buttons are still
// `buttons`; otherwise `compiled`, with the reason on stderr (a changed button list needs a build).
[[nodiscard]] std::vector<InputMapBinding> ReadInputMapBindings(std::u8string_view source,
                                                                std::span<const std::string_view> buttons,
                                                                std::vector<InputMapBinding> compiled);
