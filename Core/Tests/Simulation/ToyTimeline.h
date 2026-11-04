#pragma once
#include "Animation/Timeline.h"
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRegistry.h"

// A stage plan and a repeating boss attack on timelines, used only to test that they replay exactly. Every curve's
// result feeds the hash, so a curve that differed between builds would show.
class ToyTimelineSimulation final : public Simulation {
public:
    explicit ToyTimelineSimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;
    [[nodiscard]] std::vector<SimulationMoment> Moments() const override;

    [[nodiscard]] int Waves() const { return _waves; }
    [[nodiscard]] int Shots() const { return _shots; }

private:
    StepTimeline _stage{{"intro", 0}, {"wave", 60}, {"secondWave", 120}, {"boss", 180}, {"end", 600}};
    StepTimeline _attack{{"windup", 0}, {"fire", 30}, {"recover", 40}, {"cycle", 60}};
    int _age = 0;
    int _attackStart = -1; // The step the boss came in; -1 before that
    int _waves = 0;
    int _shots = 0;
    float _charge = 0.0f;
    double _curves = 0.0;
};

[[nodiscard]] SimulationDefinition ToyTimelineDefinition();
