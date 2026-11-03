#pragma once
#include "Modules/JsonIO/JsonIO.h"
#include "Simulation/InputTape.h"
#include "Simulation/SimulationRegistry.h"

// A JSON scenario, run without a window (format: Docs/Simulation.md):
//
//     {"simulation": "my.play", "seed": 42, "players": 2, "start": {"difficulty": 2},
//      "script": [{"hold": "Right"}, {"run": 30}, {"tap": "Fire", "player": 1}, {"run": 1},
//                 {"expect": {"score": 10, "player[0].lives": {"min": 1}}}, {"read": ["score"]}]}
struct ScenarioResult {
    bool passed = true;
    // What `<engine> sim run` prints: passed, steps, hash, failures, reads.
    JsonIO::Document report;
    InputTape tape;
};
// Throws std::invalid_argument for a script that cannot run: unknown simulation, name, player or operation.
[[nodiscard]] ScenarioResult RunScenarioScript(const SimulationRegistry& registry, const JsonIO::Document& script);
