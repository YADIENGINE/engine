// [recipe:TimelineBoss-header]
#pragma once
#include <cstdint>
#include <vector>
#include "Animation/Timeline.h"
#include "Simulation/HostedSimulation.h"
#include "Editor/Api/SceneRegistry.h"
#include "Scene/Scene.h"
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRegistry.h"

class Painter;

// The rules, in Game/Simulation. Timelines are constant data, so they stay out of State; the clocks are the state.
class RecipeBossSimulation final : public Simulation {
public:
    explicit RecipeBossSimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;
    [[nodiscard]] std::vector<SimulationMoment> Moments() const override;

    [[nodiscard]] int Shots() const { return _shots; }
    [[nodiscard]] bool BossIn() const { return _bossStart >= 0; }
    // For the view, between steps: how far the boss has charged, 0..1.
    [[nodiscard]] float Charge(float alpha) const;

private:
    StepTimeline _stage{{"intro", 0}, {"boss", StepsIn(2.0, 60)}, {"end", StepsIn(30.0, 60)}};
    StepTimeline _attack{{"charge", 0}, {"fire", 45}, {"recover", 50}, {"cycle", 90}};
    int _age = 0;
    int _bossStart = -1; // The step the boss came in; -1 before that
    int _cycle = 0;      // The attack's clock
    int _shots = 0;
};

[[nodiscard]] SimulationDefinition RecipeBossDefinition();
// The view, outside Game/Simulation: a curve shapes what the rules' timeline measures.
void DrawRecipeBoss(Painter& paint, const RecipeBossSimulation& simulation, float alpha);

class RecipeBossScene final : public Scene {
public:
    RecipeBossScene(const SceneServices& services, std::uint64_t seed);
    ~RecipeBossScene() override { ShutdownNoThrow(); }

private:
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;

    HostedSimulation<RecipeBossSimulation> _hosted;
};
[[nodiscard]] SceneDefinition RecipeBossSceneDefinition();
// [/recipe]
