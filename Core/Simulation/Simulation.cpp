#include "Simulation/Simulation.h"

Simulation::~Simulation() = default;
std::vector<SimulationMoment> Simulation::Moments() const { return {}; }
const SimulationEventLog* Simulation::EventLog() const { return nullptr; }
