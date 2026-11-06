#pragma once
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>
#include "Input/Input.h"
#include "Input/InputMap.h"
#include "Simulation/PlayerInput.h"

// Translates a frame of device input into per-player actions: a plain player, action, key table. It runs before the
// tape, so tapes hold actions and do not depend on key layout or devices. A replay does not reproduce binding mistakes;
// test bindings on their own. Axes are the game's: call SetAxis on the result before feeding it to the runner.
class InputBindings {
public:
    explicit InputBindings(int players);
    // An action may have several keys, and a key may serve several players.
    template <class Button> void Bind(int player, Button button, InputKey key) {
        AddBinding(player, static_cast<std::size_t>(button), key);
    }
    // Binds an input map's default keys (X.input.json -> XInput; Docs/InputMap.md) for the players this reads; the
    // map's other players are left out. Development builds read the file here, so a key changed in the editor applies
    // the next time a scene binds the map.
    template <class Map> void BindMap() {
        std::vector<InputMapBinding> keys;
        for (const auto& key : Map::Keys)
            if (const auto found = FindInputKey(key.key))
                keys.push_back({key.player, static_cast<std::size_t>(key.button), *found});
#if ENGINE_TUNING_LIVE
        const std::span<const std::string_view> buttons(Map::ButtonNames);
        keys = ReadInputMapBindings(Map::InputSource, buttons, std::move(keys));
#endif
        Bind(keys);
    }
    // Binds each key whose player this reads.
    void Bind(std::span<const InputMapBinding> keys);
    void Clear();
    // Borrowed until the next Read.
    [[nodiscard]] std::span<PlayerInput> Read(const DeviceInput& input);

private:
    struct Binding {
        std::size_t player = 0;
        std::size_t button = 0;
        InputKey key = InputKey::Space;
    };
    void AddBinding(int player, std::size_t button, InputKey key);
    std::vector<Binding> _bindings;
    std::vector<PlayerInput> _players;
};
