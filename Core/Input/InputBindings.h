#pragma once
#include <cstddef>
#include <span>
#include <vector>
#include "Input/Input.h"
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
