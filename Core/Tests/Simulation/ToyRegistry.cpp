#include "Simulation/SimulationRegistry.h"
#include "Tests/Simulation/ToyCollision.h"
#include "Tests/Simulation/ToySimulation.h"

void RegisterSimulations(SimulationRegistry& registry) {
    registry.Add(ToyDefinition());
    registry.Add(ToyCollisionDefinition());
}
