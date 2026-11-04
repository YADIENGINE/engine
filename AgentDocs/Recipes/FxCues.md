# Effect cues recipe

Name what an event looks like, in one place: a burst, a shake, a flash and a late shockwave become `fx.Play("player_hit", ...)`. An animation's moments and a game's rules can both play them.

APIs: Core/Fx/FxCue.h, Core/Fx/FxCueSet.h, Core/Fx/FxStage.h, Core/Runtime/TimeScale.h. Concepts: `Docs/Graphics/Effects.md#cues`.

<!-- recipe:FxCues-header -->
```cpp
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
```
<!-- /recipe -->

<!-- recipe:FxCues -->
```cpp
#include "AgentDocs/Recipes/Code/FxCues.h"
#include "Animation/Clip.h"
#include "Animation/ClipSet.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"

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

RecipeCueView::RecipeCueView(const FloatRect& area, TimeScale& time)
    : _at(area.Center())
    , _fx(area, area, CurrentTheme().fx)
    , _swing(SwingClips(), "rest") {
    _fx.SetCues(RecipeCues());
    _fx.SetTime(&time); // "player_hit" asks for a hitstop
}
void RecipeCueView::Update(float gameSeconds, float realSeconds, int hits) {
    if (hits != _hitsSeen) {
        _hitsSeen = hits;
        _fx.Play("player_hit", {.at = _at, .color = HitColor});
    }
    // A moment with a cue of the same name plays it, at the frame the animation reaches it.
    _swing.Update(gameSeconds, [&](std::string_view moment) {
        if (_fx.Cues().Contains(moment)) _fx.Play(moment, {.at = _at, .color = SwingColor});
    });
    _fx.Update(gameSeconds, realSeconds);
}
void RecipeCueView::Draw(Painter& paint) const {
    {
        auto camera = paint.Transform(_fx.WorldCamera(paint).Transform());
        // Draw the world here, using _swing.Pose().arm.
        _fx.DrawWorldLayer(paint);
    }
    _fx.DrawOverlay(paint);
}
```
<!-- /recipe -->

Pitfalls: A cue with FxHitstop or FxSlowMotion throws without SetTime. A misspelled name throws where it plays, so a scenario or test should reach each one. A simulation never calls Fx: it emits events (Simulation recipe) and the view plays cues for them. Without a simulation, comparing a total, as here, cannot play one twice. A cue's delays and cooldown run on game time, so a hitstop holds them. FxPlay::intensity sizes one play. Update takes both clocks, the scene's delta and Game::RealSeconds(). Pass Game::Effects() to SetQuality.
