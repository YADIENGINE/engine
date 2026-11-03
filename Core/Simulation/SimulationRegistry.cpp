#include "Simulation/SimulationRegistry.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

void SimulationRegistry::Add(SimulationDefinition definition) {
    ValidateSimulation(definition.settings);
    if (!definition.create) throw std::invalid_argument(definition.settings.id + ": a simulation needs a factory");
    if (Find(definition.settings.id)) throw std::invalid_argument("Repeated simulation id: " + definition.settings.id);
    _entries.push_back(std::move(definition));
}
const SimulationDefinition* SimulationRegistry::Find(std::string_view id) const {
    const auto found =
        std::ranges::find_if(_entries, [id](const SimulationDefinition& entry) { return entry.settings.id == id; });
    return found == _entries.end() ? nullptr : &*found;
}
