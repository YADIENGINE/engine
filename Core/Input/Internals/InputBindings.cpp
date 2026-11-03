#include "Input/InputBindings.h"
#include <stdexcept>

InputBindings::InputBindings(int players) {
    if (players < 1) throw std::invalid_argument("InputBindings needs at least one player");
    _players.resize(static_cast<std::size_t>(players));
}
void InputBindings::AddBinding(int player, std::size_t button, InputKey key) {
    if (player < 0 || static_cast<std::size_t>(player) >= _players.size())
        throw std::invalid_argument("InputBindings: no such player");
    if (button >= MaxPlayerButtons) throw std::invalid_argument("InputBindings: no such button");
    _bindings.push_back({static_cast<std::size_t>(player), button, key});
}
void InputBindings::Clear() { _bindings.clear(); }
std::span<PlayerInput> InputBindings::Read(const DeviceInput& input) {
    for (auto& player : _players)
        player = PlayerInput{};
    for (const auto& binding : _bindings) {
        auto& player = _players[binding.player];
        if (input.Down(binding.key)) player.SetHeld(binding.button, true);
        if (input.Pressed(binding.key)) player.SetPressed(binding.button);
    }
    return _players;
}
