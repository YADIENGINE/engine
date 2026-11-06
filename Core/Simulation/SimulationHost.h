#pragma once
#include "Simulation/SimulationRunner.h"
#include "Simulation/SimulationSettings.h"

// The editor's side of a scene that hosts a simulation: it keeps the tape across resets, so `ctl seek`, `ctl replay`
// and the toolbar work in exact steps. Scenes use HostedSimulation rather than this directly.
class SimulationHost {
public:
    virtual ~SimulationHost();
    // What to start the scene's simulation from: a loaded replay's start, otherwise 'fallback'.
    [[nodiscard]] virtual SimulationStart StartFor(const SimulationSettings& settings, SimulationStart fallback) = 0;
    // A runner borrowed until Detach. A kept tape plays on it, then input is live.
    virtual void Attach(SimulationRunner& runner) = 0;
    virtual void Detach(SimulationRunner& runner) = 0;

protected:
    SimulationHost() = default;
    SimulationHost(const SimulationHost&) = default;
    SimulationHost& operator=(const SimulationHost&) = default;
};
