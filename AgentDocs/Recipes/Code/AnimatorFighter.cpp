// [recipe:AnimatorFighter]
#include "AgentDocs/Recipes/Code/AnimatorFighter.h"
#include <memory>
#include <string_view>
#include "Graphics/Paint/PaintStyle.h"
#include "Graphics/Paint/Painter.h"
#include "Simulation/StateVisitor.h"
#include "Utilities/Float2.h"

namespace {
using FighterPose = RecipeFighterPose;
constexpr Float2 Body{400.0f, 300.0f};

// One set per kind of fighter: every animator built from it shares the same constant data.
StepClipSet<FighterPose> FighterClips() {
    return {
        {"idle", StepClip<FighterPose>{}
                     .Track(&FighterPose::bob, {{0, 0.0f}, {30, -6.0f, Curve::InOutSine}, {60, 0.0f, Curve::InOutSine}})
                     .Looping()},
        {"walk", StepClip<FighterPose>{}
                     .Track(&FighterPose::bob, {{0, 0.0f}, {10, -10.0f, Curve::OutQuad}, {20, 0.0f, Curve::InQuad}})
                     .Track(&FighterPose::frame, {{0, 0}, {10, 1}})
                     .Looping()},
        {"attack", StepClip<FighterPose>{}
                       .Track(&FighterPose::arm, {{0, 0.0f}, {12, -1.2f, Curve::InBack}, {18, 0.9f, Curve::OutQuart}, {30, 0.0f}})
                       .Track(&FighterPose::tint, {{0, Color4{120, 220, 255, 255}},
                                            {18, Color4{255, 80, 60, 255}, Curve::InQuad},
                                            {30, Color4{120, 220, 255, 255}}})
                       .Moment("hit", 18)},
    };
}
InputBindings MakeBindings() {
    InputBindings bindings(1);
    bindings.Bind(0, RecipeFighterButton::Walk, InputKey::D);
    bindings.Bind(0, RecipeFighterButton::Attack, InputKey::Space);
    return bindings;
}
} // namespace

RecipeFighterSimulation::RecipeFighterSimulation(const SimulationStart&)
    : _animator(FighterClips(), "idle") {}
void RecipeFighterSimulation::Step(const SimulationStep& step) {
    const auto& input = step.Player(0);
    if (input.Pressed(RecipeFighterButton::Attack))
        _animator.Play("attack", {.blend = 4, .then = "idle", .thenBlend = 8, .restart = true});
    // Say what should be showing every step: playing the current clip does nothing, so it needs no bookkeeping.
    if (_animator.Clip() != "attack") _animator.Play(input.Held(RecipeFighterButton::Walk) ? "walk" : "idle", {.blend = 8});

    _animator.Step([&](std::string_view moment) {
        if (moment == "hit") {
            ++_hits;
            _recoil.To(1.0f, 3, Curve::OutQuad).Then(0.0f, 12, Curve::OutBounce);
        }
    });
    _recoil.Step();
}
void RecipeFighterSimulation::State(StateVisitor& state) const {
    state.Field("hits", _hits);
    state.Group("animator", [&] { _animator.State(state); });
    state.Group("recoil", [&] { _recoil.State(state); });
}
SimulationDefinition RecipeFighterDefinition() {
    SimulationDefinition definition;
    definition.settings.id = "myproject.fighter";
    definition.settings.buttons = {"Walk", "Attack"};
    definition.create = [](const SimulationStart& start) { return std::make_unique<RecipeFighterSimulation>(start); };
    return definition;
}

void DrawRecipeFighter(Painter& paint, const RecipeFighterSimulation& simulation, float alpha) {
    const FighterPose pose = simulation.Pose(alpha);
    const Float2 at = Body + Float2{-12.0f * simulation.Recoil(alpha), pose.bob};
    paint.Circle(at, 40.0f, Fill(pose.tint));
    paint.Circle(at + Rotated(Float2{60.0f, 0.0f}, pose.arm), 12.0f, Fill(pose.tint));
}

RecipeFighterEditorScene::RecipeFighterEditorScene(const SceneServices& services, std::uint64_t seed)
    : _hosted(services.simulation, RecipeFighterDefinition().settings, SimulationStart{.seed = seed, .players = 1})
    , _bindings(MakeBindings()) {}
void RecipeFighterEditorScene::HandleInput(const DeviceInput& input) {
    if (input.cancelled)
        _hosted.Runner().CancelInput();
    else
        _hosted.Runner().Feed(_bindings.Read(input));
}
void RecipeFighterEditorScene::Update(float seconds) { (void)_hosted.Runner().Advance(seconds); }
void RecipeFighterEditorScene::Draw(Painter& paint) const {
    DrawRecipeFighter(paint, _hosted.Sim(), _hosted.Runner().Alpha());
}
SceneDefinition RecipeFighterSceneDefinition() {
    SceneDefinition definition;
    definition.id = "myproject.fighter";
    definition.name = "Fighter";
    definition.category = "MyProject";
    definition.create = [](const SceneServices& services, const SceneParameters&) {
        return std::make_unique<RecipeFighterEditorScene>(services, 1);
    };
    return definition;
}
// [/recipe]
