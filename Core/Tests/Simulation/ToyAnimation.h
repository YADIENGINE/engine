#pragma once
#include <vector>
#include "Animation/Animator.h"
#include "Animation/Tween.h"
#include "Animation/TweenSet.h"
#include "Simulation/Simulation.h"
#include "Simulation/SimulationRegistry.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Random/Rng.h"

struct ToyPose {
    float arm = 0.0f;
    float scale = 1.0f;
    Float2 offset{};
    Color4 tint{255, 255, 255, 255};
    int frame = 0;
    static constexpr auto Fields() {
        return PoseFields(PoseField("arm", &ToyPose::arm), PoseField("scale", &ToyPose::scale),
                          PoseField("offset", &ToyPose::offset), PoseField("tint", &ToyPose::tint),
                          PoseField("frame", &ToyPose::frame));
    }
};

// Animators, tweens and tween sets deciding a game's rules, used only to test that they replay exactly: random clip
// changes with fades that get interrupted, and every curve through a pose. What they produce feeds the hash.
class ToyAnimationSimulation final : public Simulation {
public:
    explicit ToyAnimationSimulation(const SimulationStart& start);
    void Step(const SimulationStep& step) override;
    void State(StateVisitor& state) const override;

    [[nodiscard]] int Shots() const { return _shots; }
    [[nodiscard]] int Interrupted() const { return _interrupted; }

private:
    struct Unit {
        StepAnimator<ToyPose> animator;
        StepTween<float> speed{1.0f};
    };
    Rng _rng;
    std::vector<Unit> _units;
    StepTween<Float2> _camera;
    StepTween<Color4> _flash;
    StepTweenSet<int> _marks{0.5f};
    int _age = 0;
    int _shots = 0;
    int _finished = 0;
    int _interrupted = 0;
    double _shown = 0.0;
};

[[nodiscard]] SimulationDefinition ToyAnimationDefinition();
