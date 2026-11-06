#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "Modules/JsonIO/JsonIO.h"

// What a simulation declares about itself; constant for all its runs. Tapes store it and refuse a different id, step
// rate or input schema.
struct SimulationSettings {
    std::string id;
    // Raise when a change makes old tapes play differently; a different version is reported, and the hashes decide.
    int version = 1;
    int stepsPerSecond = 60; // 1..240; slow motion lowers steps per real second, not this
    int maxPlayers = 1;
    std::vector<std::string> buttons; // At most MaxPlayerButtons; a button's position is its index
    std::vector<std::string> axes;    // At most MaxPlayerAxes
};

// Buttons and axes from an input map (X.input.json -> XInput; Docs/InputMap.md), in its order, so the enum and the
// names cannot drift apart.
template <class Map> void DeclareInput(SimulationSettings& settings) {
    settings.buttons.clear();
    for (const auto name : Map::ButtonNames)
        settings.buttons.emplace_back(name);
    settings.axes.clear();
    for (const auto name : Map::AxisNames)
        settings.axes.emplace_back(name);
}

// The most game time one frame can owe a runner: time beyond it is dropped (SimulationRunner::Advance). Events are
// kept at least this long, so a view that reads them every frame misses none (SimulationEvents).
inline constexpr double SimulationMaxFrameSeconds = 0.25;

// What a run starts from: the only inputs besides the per-step ones. Anything else that changes gameplay (options,
// saved progress, menu choices) must arrive here or through a command, or a replay cannot reproduce it.
struct SimulationStart {
    std::uint64_t seed = 0;
    int players = 1;
    JsonIO::Document parameters = JsonIO::Document::MakeObject();
    friend bool operator==(const SimulationStart&, const SimulationStart&) = default;
};

// Throws std::invalid_argument when a declaration or a start is outside its limits.
void ValidateSimulation(const SimulationSettings& settings);
void ValidateSimulationStart(const SimulationSettings& settings, const SimulationStart& start);
