#pragma once
#include "Simulation/SimulationStep.h"

class StateVisitor;

// A game's rules as plain state: no window, drawing, audio, clocks or global random generators. A scene hosts it
// through SimulationRunner and draws its state; headless tools run it without a scene. Given the same start and the
// same steps, the state must end up the same.
class Simulation {
public:
    virtual ~Simulation();
    virtual void Step(const SimulationStep& step) = 0;
    // Names every field that decides later steps, a Rng included. One function feeds the hash, dumps and scenario
    // checks.
    virtual void State(StateVisitor& state) const = 0;

protected:
    Simulation() = default;
    Simulation(const Simulation&) = default;
    Simulation& operator=(const Simulation&) = default;
};
