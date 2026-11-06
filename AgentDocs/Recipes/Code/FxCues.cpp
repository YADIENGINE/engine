// [recipe:FxCues]
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
// [/recipe]
