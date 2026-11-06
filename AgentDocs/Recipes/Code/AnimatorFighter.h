// [recipe:AnimatorFighter-header]
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

private:
    void HandleInput(const DeviceInput& input) override;
    void InputCancelled() override;
    void Update(float seconds) override;
    void Draw(Painter& paint) const override;

    HostedSimulation<RecipeFighterSimulation> _hosted;
    InputBindings _bindings;
};
[[nodiscard]] SceneDefinition RecipeFighterSceneDefinition();
// [/recipe]
