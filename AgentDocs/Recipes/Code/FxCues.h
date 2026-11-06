// [recipe:FxCues-header]
#pragma once
#include "Animation/Animator.h"
#include "Stage/Stage.h"

struct RecipeSwingPose {
    float arm = 0.0f;
};

// A view that names what events look like, and lets an animation's moments play them. The stage's effects follow the
// player's choice and use the scene's time scale for the hitstop.
class RecipeCueView final : public StageView {
public:
    explicit RecipeCueView(const SceneServices& services);
    void Swing() { _swing.Play("swing", {.restart = true}); }
    // Each frame, from the scene's UpdateReal.
    void UpdateReal(float realSeconds) { _stage.UpdateReal(realSeconds); }
    // Each frame, from the scene's Update. 'hits' is what a game's rules counted so far, a simulation's for example:
    // comparing a total cannot miss one.
    void Update(float seconds, int hits);
    void Draw(Painter& paint) const { _stage.Draw(paint, *this); }

private:
    void DrawWorld(Painter& paint, const StagePass& pass) const override;
    Float2 _at;
    Stage _stage;
    SecondsAnimator<RecipeSwingPose> _swing;
    int _hitsSeen = 0;
};
// [/recipe]
