# Timeline recipe

Time a stage and a repeating boss attack with named moments, and shape what the view draws with a curve, inside a simulation that replays exactly.

APIs: Core/Animation/Timeline.h, Core/Animation/Easing.h, Core/Simulation/TimelineMoments.h. Concepts: [Animation](../../Docs/Animation.md).

Put the rules in Game/Simulation/ and implement `RegisterSimulations` there: both headers are available and need no CMake line. Views and scenes stay outside that folder. This example keeps them in one file only to read in one place.

<!-- recipe:TimelineBoss-header -->
```cpp
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

private:
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;

    HostedSimulation<RecipeBossSimulation> _hosted;
};
[[nodiscard]] SceneDefinition RecipeBossSceneDefinition();
```
<!-- /recipe -->

<!-- recipe:TimelineBoss -->
```cpp
#include "AgentDocs/Recipes/Code/TimelineBoss.h"
#include <memory>
#include <string_view>
#include "Animation/Easing.h"
#include "Graphics/Paint/PaintStyle.h"
#include "Graphics/Paint/Painter.h"
#include "Simulation/StateVisitor.h"
#include "Simulation/TimelineMoments.h"
#include "Utilities/Math/Scalar.h"

namespace {
constexpr Float2 BossPosition{400.0f, 200.0f};
constexpr Color4 BossColor{255, 120, 90, 255};
} // namespace

RecipeBossSimulation::RecipeBossSimulation(const SimulationStart&) {}
void RecipeBossSimulation::Step(const SimulationStep&) {
    _stage.ForEachCrossed(_age, _age + 1, [&](std::string_view moment) {
        if (moment == "boss") _bossStart = _age;
    });
    if (BossIn()) {
        _cycle = _attack.Wrap(_age - _bossStart);
        _attack.ForEachCrossed(_cycle, _cycle + 1, [&](std::string_view moment) {
            if (moment == "fire") ++_shots;
        });
    }
    ++_age;
}
void RecipeBossSimulation::State(StateVisitor& state) const {
    state.Field("age", _age);
    state.Field("phase", _stage.PhaseAt(_age));
    state.Field("bossStart", _bossStart);
    state.Field("cycle", _cycle);
    state.Field("attackPhase", _attack.PhaseAt(_cycle));
    state.Field("shots", _shots);
}
std::vector<SimulationMoment> RecipeBossSimulation::Moments() const {
    std::vector<SimulationMoment> moments;
    AppendMoments(moments, _stage);
    // The first attack's steps are known once the boss has come in.
    if (BossIn()) AppendMoments(moments, _attack, static_cast<std::uint64_t>(_bossStart), "attack.");
    return moments;
}
float RecipeBossSimulation::Charge(float alpha) const {
    return BossIn() ? _attack.Progress("charge", "fire", _cycle, alpha) : 0.0f;
}
SimulationDefinition RecipeBossDefinition() {
    SimulationDefinition definition;
    definition.settings.id = "myproject.boss";
    definition.create = [](const SimulationStart& start) { return std::make_unique<RecipeBossSimulation>(start); };
    return definition;
}

void DrawRecipeBoss(Painter& paint, const RecipeBossSimulation& simulation, float alpha) {
    if (!simulation.BossIn()) return;
    paint.Circle(BossPosition, Mix(30.0f, 60.0f, EaseOutBack(simulation.Charge(alpha))), Fill(BossColor));
}

RecipeBossScene::RecipeBossScene(const SceneServices& services, std::uint64_t seed)
    : _hosted(services.simulation, RecipeBossDefinition().settings, SimulationStart{.seed = seed, .players = 1}) {}
void RecipeBossScene::Update(float seconds) { (void)_hosted.Runner().Advance(seconds); }
void RecipeBossScene::Draw(Painter& paint) const { DrawRecipeBoss(paint, _hosted.Sim(), _hosted.Runner().Alpha()); }
SceneDefinition RecipeBossSceneDefinition() {
    SceneDefinition definition;
    definition.id = "myproject.boss";
    definition.name = "Boss";
    definition.category = "MyProject";
    definition.create = [](const SceneServices& services, const SceneParameters&) {
        return std::make_unique<RecipeBossScene>(services, 1);
    };
    return definition;
}
```
<!-- /recipe -->

Test it without a window: `python Engine sim run boss.json`.

```json
{ "simulation": "myproject.boss", "seed": 1, "script": [
  { "expect": { "phase": "intro", "shots": 0 } },
  { "run": 120 }, { "expect": { "phase": "boss", "bossStart": -1 } },
  { "run": 46 }, { "expect": { "bossStart": 120, "attackPhase": "fire", "shots": 1 } } ] }
```

See it: `python Engine ctl open myproject.boss`, then `python Engine ctl status` lists `moments` and `python Engine ctl seek --moment boss` jumps there.

Checks:

- The clock is the only state. The timelines are constant, so they stay out of `State`.
- `ForEachCrossed(age, age + 1, ...)` comes before the clock moves. With that order each moment is met once, a moment at 0 included.
- After 120 steps, `phase` already says `boss`, but the step that meets it has not run yet: `bossStart` is still -1. This one-step difference is normal; check the field the rule changes.
- `Wrap` never meets the last moment of a cycle. `cycle` here only marks the length.
- The view does not change anything: `Charge` only reads the clock with `alpha`.
