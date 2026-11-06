# Effect cues recipe

Name what an event looks like, in one place: a burst, a shake, a flash and a late shockwave become `fx.Play("player_hit", ...)`. An animation's moments and a game's rules can both play them.

APIs: Core/Fx/FxCue.h, Core/Fx/FxCueSet.h, Core/Fx/FxStage.h, Core/Stage/Stage.h (`Fx`). Concepts: `Docs/Graphics/Effects.md#cues`.

<!-- recipe:FxCues-header -->
```cpp
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
```
<!-- /recipe -->

<!-- recipe:FxCues -->
```cpp
#include "AgentDocs/Recipes/Code/FxCues.h"
#include <cmath>
#include "Animation/Clip.h"
#include "Animation/ClipSet.h"
#include "Graphics/Paint/Painter.h"
#include "Scene/SceneServices.h"

namespace {
constexpr Color4 HitColor = Rgb(0xff5040);
constexpr Color4 SwingColor = Rgb(0xffd040);
constexpr Color4 FlashColor = Rgb(0xffffff);

// Data, so the look of an event is in one place. The default cues stay available; a name here would replace one.
FxCueSet RecipeCues() {
    return DefaultFxCues().With({
        {"player_hit",
         {FxBurst{ParticleRecipe::Debris, 24}, FxShake{0.5f}, FxHitstop{0.05f}, {FxFlash{}, FlashColor},
          {0.08f, FxShockwave{120.0f}}},
         0.1f}, // At most one every 0.1 seconds, however many land
        {"swing_land", {FxBurst{ParticleRecipe::Spark, 12}, FxRing{}, FxPunch{0.03f}}},
    });
}
SecondsClipSet<RecipeSwingPose> SwingClips() {
    return {{"rest", SecondsClip<RecipeSwingPose>{}},
            {"swing", SecondsClip<RecipeSwingPose>{}
                          .Track(&RecipeSwingPose::arm, {{0.0f, 0.0f}, {0.2f, -1.2f, Curve::InBack}, {0.4f, 0.0f}})
                          .Moment("swing_land", 0.2f)}};
}
} // namespace

RecipeCueView::RecipeCueView(const SceneServices& services)
    : _at(ToFloat2(services.canvas) * 0.5f)
    , _stage(services)
    , _swing(SwingClips(), "rest") {
    _stage.Fx().SetCues(RecipeCues());
}
void RecipeCueView::Update(float seconds, int hits) {
    if (hits != _hitsSeen) {
        _hitsSeen = hits;
        _stage.Fx().Play("player_hit", {.at = _at, .color = HitColor});
    }
    // A moment with a cue of the same name plays it, at the frame the animation reaches it.
    _swing.Update(seconds, [&](std::string_view moment) {
        if (_stage.Fx().Cues().Contains(moment)) _stage.Fx().Play(moment, {.at = _at, .color = SwingColor});
    });
    _stage.Update(seconds);
}
void RecipeCueView::DrawWorld(Painter& paint, const StagePass&) const {
    const Float2 hand = _at + Float2{std::cos(_swing.Pose().arm), std::sin(_swing.Pose().arm)} * 60.0f;
    paint.Line(_at, hand, {.width = 8.0f, .color = SwingColor});
}
```
<!-- /recipe -->

Pitfalls: A stage gives its effects the scene's time scale, so FxHitstop and FxSlowMotion work; an FxStage of your own throws without SetTime. A misspelled name throws where it plays, so a scenario or test should reach each one. A simulation never calls Fx: it emits events (Simulation recipe) and the view plays cues for them. Without a simulation, comparing a total, as here, cannot play one twice. A cue's delays and cooldown run on game time, so a hitstop holds them. FxPlay::intensity sizes one play. The stage has two calls, `UpdateReal` from the scene's `UpdateReal` (it runs in a hitstop, so banners and toasts go on) and `Update` from its `Update`, and follows the player's effect choice by itself.
