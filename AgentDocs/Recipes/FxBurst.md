Emit themed particles, camera shake and a flash on a stage's effects.

APIs: Core/Fx/FxStage.h, Core/Stage/Stage.h (`Fx`), Core/Graphics/Style/Theme.h.

Trigger on an event with `TriggerRecipeFx(_stage.Fx(), at)`. The scene's `Stage` updates the effects on their clocks (`UpdateReal`, `Update`) and draws them in its passes: the burst over the world, the shake in every view, the flash over the canvas.

<!-- recipe:FxBurst -->
```cpp
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
```
<!-- /recipe -->

Pitfalls: Positions are world positions under a game camera. The stage follows Game::Effects() by itself. Call `Fx().Clear()` on a scene or run reset. An FxStage without a Stage draws `DrawGroundLayer` and `DrawWorldLayer` inside the world camera (`WorldCamera`) and `DrawOverlay` after it.

Concept: `Docs/Graphics/Effects.md#effect-stage-and-particle-recipes`.
