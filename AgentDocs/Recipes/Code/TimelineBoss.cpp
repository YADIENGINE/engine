// [recipe:TimelineBoss]
#include "AgentDocs/Recipes/Code/TimelineBoss.h"
#include <memory>
#include <string_view>
#include "Animation/Easing.h"
#include "Graphics/Paint/PaintStyle.h"
#include "Graphics/Paint/Painter.h"
#include "Simulation/StateVisitor.h"
#include "Simulation/TimelineMoments.h"
#include "Utilities/MathUtils.h"

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

RecipeBossEditorScene::RecipeBossEditorScene(const SceneServices& services, std::uint64_t seed)
    : _hosted(services.simulation, RecipeBossDefinition().settings, SimulationStart{.seed = seed, .players = 1}) {}
void RecipeBossEditorScene::Update(float seconds) { (void)_hosted.Runner().Advance(seconds); }
void RecipeBossEditorScene::Draw(Painter& paint) const { DrawRecipeBoss(paint, _hosted.Sim(), _hosted.Runner().Alpha()); }
SceneDefinition RecipeBossSceneDefinition() {
    SceneDefinition definition;
    definition.id = "myproject.boss";
    definition.name = "Boss";
    definition.category = "MyProject";
    definition.create = [](const SceneServices& services, const SceneParameters&) {
        return std::make_unique<RecipeBossEditorScene>(services, 1);
    };
    return definition;
}
// [/recipe]
