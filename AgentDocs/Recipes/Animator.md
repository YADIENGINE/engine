# Animator recipe

A character with clips that fade into each other, a moment that decides gameplay and a tween for a value no clip knows, inside a simulation that replays exactly.

APIs: Core/Animation/Animator.h, Core/Animation/Clip.h, Core/Animation/ClipSet.h, Core/Animation/Pose.h, Core/Animation/Tween.h. Concepts: [Animation](../../Docs/Animation.md).

Put the rules in Game/Simulation/ and implement `RegisterSimulations` there: the headers are available and need no CMake line. Views and scenes stay outside that folder. This example keeps them in one file only to read in one place.

<!-- recipe:AnimatorFighter-header -->
```cpp
#pragma once
#include <cstdint>
#include "Animation/Animator.h"
#include "Animation/Tween.h"
#include "Simulation/HostedSimulation.h"
#include "Editor/Api/SceneRegistry.h"
#include "Scene/Scene.h"
#include "Input/InputBindings.h"
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRegistry.h"
#include "Utilities/Color4.h"

class Painter;

enum class RecipeFighterButton { Walk, Attack };

// What animates. The default values are the rest pose; every field blends and reaches State by its own name.
struct RecipeFighterPose {
    float arm = 0.0f;
    float bob = 0.0f;
    Color4 tint{120, 220, 255, 255};
    int frame = 0;
};

// The rules, in Game/Simulation. They decide what plays; the animator keeps track of how far it is.
class RecipeFighterSimulation final : public Simulation {
public:
    explicit RecipeFighterSimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;

    [[nodiscard]] int Hits() const { return _hits; }
    [[nodiscard]] RecipeFighterPose Pose(float alpha) const { return _animator.Pose(alpha); }
    [[nodiscard]] float Recoil(float alpha) const { return _recoil.Value(alpha); }

private:
    StepAnimator<RecipeFighterPose> _animator;
    StepTween<float> _recoil{0.0f}; // A value with no clip: it goes wherever a hit sends it
    int _hits = 0;
};

[[nodiscard]] SimulationDefinition RecipeFighterDefinition();
// The view, outside Game/Simulation: it draws a pose and decides nothing.
void DrawRecipeFighter(Painter& paint, const RecipeFighterSimulation& simulation, float alpha);

class RecipeFighterScene final : public Scene {
public:
    RecipeFighterScene(const SceneServices& services, std::uint64_t seed);
    ~RecipeFighterScene() override { ShutdownNoThrow(); }

private:
    void HandleInput(const DeviceInput& input) override;
    void InputCancelled() override;
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;

    HostedSimulation<RecipeFighterSimulation> _hosted;
    InputBindings _bindings;
};
[[nodiscard]] SceneDefinition RecipeFighterSceneDefinition();
```
<!-- /recipe -->

<!-- recipe:AnimatorFighter -->
```cpp
#include "AgentDocs/Recipes/Code/AnimatorFighter.h"
#include <memory>
#include <string_view>
#include "Graphics/Paint/PaintStyle.h"
#include "Graphics/Paint/Painter.h"
#include "Simulation/StateVisitor.h"
#include "Utilities/Geometry/Float2.h"

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
    state.Field("animator", _animator); // What its own State writes, as a group
    state.Field("recoil", _recoil);
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

RecipeFighterScene::RecipeFighterScene(const SceneServices& services, std::uint64_t seed)
    : _hosted(services.simulation, RecipeFighterDefinition().settings, SimulationStart{.seed = seed, .players = 1})
    , _bindings(MakeBindings()) {}
void RecipeFighterScene::HandleInput(const DeviceInput& input) { _hosted.Runner().Feed(_bindings.Read(input)); }
void RecipeFighterScene::InputCancelled() { _hosted.Runner().CancelInput(); }
void RecipeFighterScene::Update(float seconds) { (void)_hosted.Runner().Advance(seconds); }
void RecipeFighterScene::Draw(Painter& paint) const {
    DrawRecipeFighter(paint, _hosted.Sim(), _hosted.Runner().Alpha());
}
SceneDefinition RecipeFighterSceneDefinition() {
    SceneDefinition definition;
    definition.id = "myproject.fighter";
    definition.name = "Fighter";
    definition.category = "MyProject";
    definition.create = [](const SceneServices& services, const SceneParameters&) {
        return std::make_unique<RecipeFighterScene>(services, 1);
    };
    return definition;
}
```
<!-- /recipe -->

Test it without a window: `python Engine sim run fighter.json`.

```json
{ "simulation": "myproject.fighter", "seed": 1, "script": [
  { "expect": { "animator.clip": "idle", "hits": 0 } },
  { "tap": "Attack" }, { "run": 1 }, { "expect": { "animator.clip": "attack" } },
  { "run": 18 }, { "expect": { "hits": 1 } },
  { "run": 11 }, { "expect": { "animator.clip": "idle" } } ] }
```

See it: `python Engine ctl open myproject.fighter`.

Checks:

- `.restart = true` lets a second attack start over. Without it, playing the current clip does nothing, which is what makes the `walk` or `idle` line safe to run every step.
- The rules read `Clip()`, so they never leave an attack early. The clip's `then` brings back `idle`, and the next step's line takes over from there.
- A moment reaches the callback after the animator has moved, so the callback may call `Play`.
- `Pose` is worked out from clips and clocks. Only `_animator` goes in `State`, not the pose.
- `sim state` lists `animator.clip`, `animator.clock` and, during a fade, `animator.blend.clock`.
