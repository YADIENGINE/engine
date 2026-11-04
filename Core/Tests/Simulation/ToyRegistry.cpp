#include "Simulation/SimulationRegistry.h"
#include "Tests/Simulation/ToyAnimation.h"
#include "Tests/Simulation/ToyCollision.h"
#include "Tests/Simulation/ToySimulation.h"
#include "Tests/Simulation/ToyTimeline.h"

void RegisterSimulations(SimulationRegistry& registry) {
    registry.Add(ToyDefinition());
    registry.Add(ToyCollisionDefinition());
    registry.Add(ToyTimelineDefinition());
    registry.Add(ToyAnimationDefinition());
}
