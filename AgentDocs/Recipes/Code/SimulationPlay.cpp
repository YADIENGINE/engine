// [recipe:SimulationPlay]
#include "AgentDocs/Recipes/Code/SimulationPlay.h"
#include <cmath>
#include <memory>
#include "Graphics/Paint/Painter.h"
#include "Input/Input.h"
#include "Simulation/StateVisitor.h"
#include "Utilities/Overloaded.h"

namespace {
constexpr float ShipSpeed = 240.0f;
constexpr float FieldWidth = 800.0f;
constexpr float HitRange = 30.0f;
constexpr float DrawY = 400.0f;
constexpr float DrawSize = 16.0f;
constexpr float HitstopSeconds = 0.05f;
constexpr Color4 ShipColor{120, 220, 255, 255};
constexpr Color4 RockColor{255, 180, 90, 255};

void DrawField(Painter& paint, float shipX, float rockX) {
    paint.Circle({shipX, DrawY}, DrawSize, Fill(ShipColor));
    paint.Circle({rockX, DrawY - DrawSize * 4.0f}, DrawSize, Fill(RockColor));
}
InputBindings MakeBindings() {
    InputBindings bindings(1);
    bindings.Bind(0, RecipeButton::Left, InputKey::A);
    bindings.Bind(0, RecipeButton::Right, InputKey::D);
    bindings.Bind(0, RecipeButton::Fire, InputKey::Space);
    return bindings;
}
} // namespace

RecipePlaySimulation::RecipePlaySimulation(const SimulationStart& start)
    : _run{.rng = Rng(start.seed)} {
    _run.rockX = _run.rng.Float(0.0f, FieldWidth);
}
void RecipePlaySimulation::Step(const SimulationStep& step) {
    const auto& input = step.Player(0);
    float direction = 0.0f;
    if (input.Held(RecipeButton::Left)) direction -= 1.0f;
    if (input.Held(RecipeButton::Right)) direction += 1.0f;
    _run.shipX.Set(_run.shipX.Current() + direction * ShipSpeed * step.seconds);
    if (input.Pressed(RecipeButton::Fire)) {
        if (std::fabs(_run.shipX.Current() - _run.rockX) < HitRange) {
            ++_run.score;
            _events.Emit(step, RecipeHit{_run.rockX});
            _run.rockX = _run.rng.Float(0.0f, FieldWidth);
        } else {
            _events.Emit(step, RecipeMiss{});
        }
    }
    for (const auto& command : step.commands)
        if (command.id == "add_score") _run.score += static_cast<int>(command.value);
}
void RecipePlaySimulation::State(StateVisitor& state) const { state.Fields(_run); } // rng, shipX, rockX, score
const SimulationSettings& RecipePlaySettings() {
    static const SimulationSettings settings = [] {
        SimulationSettings made;
        made.id = "myproject.play";
        made.buttons = {"Left", "Right", "Fire"};
        return made;
    }();
    return settings;
}
SimulationDefinition RecipePlayDefinition() {
    return {RecipePlaySettings(),
            [](const SimulationStart& start) { return std::make_unique<RecipePlaySimulation>(start); }};
}

RecipePlayScene::RecipePlayScene(TimeScale& time, std::uint64_t seed)
    : Scene(TimeDomain::Game)
    , _time(time)
    , _start{.seed = seed, .players = 1}
    , _simulation(_start)
    , _runner(_simulation, RecipePlaySettings(), _start)
    , _bindings(MakeBindings()) {}
RecipePlayScene::~RecipePlayScene() { ShutdownNoThrow(); }
void RecipePlayScene::HandleInput(const DeviceInput& input) { _runner.Feed(_bindings.Read(input)); }
void RecipePlayScene::InputCancelled() { _runner.CancelInput(); }
void RecipePlayScene::Update(float seconds) {
    (void)_runner.Advance(seconds); // TimeScale already applied: hitstop and slow motion only change steps per second
    // Every event of the steps this frame ran, in order; none of the steps a seek or RunTo skipped.
    for (const auto& happened : _events.Read(_simulation.Events(), _runner))
        std::visit(Overloaded{[&](const RecipeHit&) { _time.Hitstop(HitstopSeconds); },
                              [](const RecipeMiss&) {}}, // A miss sound would play here
                   happened.event);
}
void RecipePlayScene::Draw(Painter& paint) const {
    DrawField(paint, _simulation.ShipX(_runner.Alpha()), _simulation.RockX());
}

RecipePlayEditorScene::RecipePlayEditorScene(const SceneServices& services, std::uint64_t seed)
    : _hosted(services.simulation, RecipePlaySettings(), SimulationStart{.seed = seed, .players = 1})
    , _bindings(MakeBindings()) {}
void RecipePlayEditorScene::HandleInput(const DeviceInput& input) {
    if (input.cancelled)
        _hosted.Runner().CancelInput();
    else
        _hosted.Runner().Feed(_bindings.Read(input));
}
std::string RecipePlayEditorScene::ExecuteAction(const std::string& id) {
    if (id == "add_score") _hosted.Runner().Command("add_score", 5); // On the tape, so a rewind plays it again
    return {};
}
void RecipePlayEditorScene::Update(float seconds) { (void)_hosted.Runner().Advance(seconds); }
void RecipePlayEditorScene::Draw(Painter& paint) const {
    DrawField(paint, _hosted.Sim().ShipX(_hosted.Runner().Alpha()), _hosted.Sim().RockX());
}
SceneDefinition RecipePlaySceneDefinition() {
    SceneDefinition definition;
    definition.id = "myproject.play";
    definition.name = "Play";
    definition.category = "MyProject";
    definition.parameters = {
        {"seed", "Seed", SceneParameterKind::Integer, std::int64_t{1}, 0.0, 1000000.0, SceneApplyMode::OnReset}};
    definition.actions = {{"add_score", "Add score", "A command on the tape.", true}};
    definition.create = [](const SceneServices& services, const SceneParameters& parameters) {
        return std::make_unique<RecipePlayEditorScene>(
            services, static_cast<std::uint64_t>(std::get<std::int64_t>(parameters.at("seed"))));
    };
    return definition;
}
// [/recipe]
