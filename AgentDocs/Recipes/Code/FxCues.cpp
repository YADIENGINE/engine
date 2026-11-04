// [recipe:FxCues]
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
         {FxBurst{ParticleRecipe::Debris, 24}, FxShake{0.2f}, FxHitstop{0.05f}, {FxFlash{}, FlashColor},
          {0.08f, FxShockwave{120.0f}}}},
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
void RecipeCueView::Update(float seconds, int hits) {
    if (hits != _hitsSeen) {
        _hitsSeen = hits;
        _fx.Play("player_hit", {.at = _at, .color = HitColor});
    }
    // A moment with a cue of the same name plays it, at the frame the animation reaches it.
    _swing.Update(seconds, [&](std::string_view moment) {
        if (_fx.Cues().Contains(moment)) _fx.Play(moment, {.at = _at, .color = SwingColor});
    });
    _fx.Update(seconds);
}
void RecipeCueView::Draw(Painter& paint) const {
    {
        auto camera = paint.Transform(_fx.WorldCamera(paint).Transform());
        // Draw the world here, using _swing.Pose().arm.
        _fx.DrawWorldLayer(paint);
    }
    _fx.DrawOverlay(paint);
}
// [/recipe]
