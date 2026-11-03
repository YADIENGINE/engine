#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include "Simulation/PlayerInput.h"

// An event that is not a key, such as choosing an upgrade in a shop. It reaches the next step and is recorded with it.
struct SimulationCommand {
    std::string id;
    std::int64_t value = 0;
    friend bool operator==(const SimulationCommand&, const SimulationCommand&) = default;
};

// One fixed step. Borrowed for the call.
struct SimulationStep {
    std::uint64_t index = 0; // 0 for the first step of a run
    float seconds = 0.0f;    // Fixed: 1 / stepsPerSecond
    std::span<const PlayerInput> players;
    std::span<const SimulationCommand> commands;

    [[nodiscard]] const PlayerInput& Player(std::size_t player) const { return players[player]; }
};
