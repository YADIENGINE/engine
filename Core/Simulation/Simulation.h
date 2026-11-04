#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "Simulation/SimulationStep.h"

class StateVisitor;

// A named point of a run, for tools: `ctl seek --moment NAME`.
struct SimulationMoment {
    std::string name;
    std::uint64_t step = 0;
};

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
    // The named points known at this step, for tools to jump to; never read by the rules. A moment whose start depends
    // on how the run goes appears once it is decided. AppendMoments (Simulation/TimelineMoments.h) lists a timeline.
    [[nodiscard]] virtual std::vector<SimulationMoment> Moments() const;

protected:
    Simulation() = default;
    Simulation(const Simulation&) = default;
    Simulation& operator=(const Simulation&) = default;
};
