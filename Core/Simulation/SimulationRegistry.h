#pragma once
#include <functional>
#include <memory>
#include <string_view>
#include <vector>
#include "Simulation/Simulation.h"
#include "Simulation/SimulationSettings.h"

// One simulation a project offers to tools, tests and the editor.
struct SimulationDefinition {
    SimulationSettings settings;
    std::function<std::unique_ptr<Simulation>(const SimulationStart&)> create;
};

class SimulationRegistry {
public:
    // Throws std::invalid_argument for an invalid declaration or a repeated id.
    void Add(SimulationDefinition definition);
    [[nodiscard]] const std::vector<SimulationDefinition>& Entries() const { return _entries; }
    [[nodiscard]] const SimulationDefinition* Find(std::string_view id) const;

private:
    std::vector<SimulationDefinition> _entries;
};

// Implemented by the project, linked into its console tool (engine_add_simulation).
void RegisterSimulations(SimulationRegistry& registry);
