// [recipe:FxBurst]
#include "AgentDocs/Recipes/Code/FxBurst.h"

namespace {
constexpr Color4 BurstColor = Rgb(0xffa040);
constexpr int BurstCount = 24;
constexpr float ShakeAmount = 0.5f;
constexpr Color4 FlashColor = Rgb(0xffffff);
} // namespace

// On an event, at a world position: the stage's UpdateReal and Update move them on their clocks, and its Draw puts the
// burst over the world, the shake into every view and the flash over the canvas.
void TriggerRecipeFx(FxStage& fx, Float2 at) {
    fx.Burst(at, BurstColor, BurstCount, ParticleRecipe::Debris);
    fx.Shake(ShakeAmount);
    fx.Flash(FlashColor);
}
// [/recipe]
