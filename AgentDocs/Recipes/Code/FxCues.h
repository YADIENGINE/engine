// [recipe:FxCues-header]
#pragma once
#include "Animation/Animator.h"
#include "Fx/FxStage.h"

class TimeScale;

struct RecipeSwingPose {
    float arm = 0.0f;
    static constexpr auto Fields() { return PoseFields(PoseField("arm", &RecipeSwingPose::arm)); }
};

// A view that names what events look like, and lets an animation's moments play them.
class RecipeCueView {
public:
    RecipeCueView(const FloatRect& area, TimeScale& time);
    void SetQuality(const FxQuality& quality) { _fx.SetQuality(quality); }
    void Swing() { _swing.Play("swing", {.restart = true}); }
    // 'hits' is what a game's rules counted so far, a simulation's for example: comparing a total cannot miss one.
    void Update(float gameSeconds, float realSeconds, int hits);
    void Draw(Painter& paint) const;

private:
    Float2 _at;
    FxStage _fx;
    SecondsAnimator<RecipeSwingPose> _swing;
    int _hitsSeen = 0;
};
// [/recipe]
