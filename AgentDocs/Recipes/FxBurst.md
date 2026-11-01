Emit themed particles, camera shake and a flash in one Fx stage.

APIs: Core/Fx/FxStage.h, Core/Graphics/Style/Theme.h, Core/Graphics/Paint/Painter.h.

Own MakeRecipeFx(area) in the View; trigger on an event, update with the chosen time domain, draw world then overlay.

<!-- recipe:FxBurst -->
```cpp
#include "AgentDocs/Recipes/Code/FxBurst.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"

namespace {
constexpr Color4 BurstColor = Rgb(0xffa040);
constexpr int BurstCount = 24;
constexpr float ShakeSeconds = 0.2f;
constexpr Color4 FlashColor = Rgb(0xffffff);
} // namespace

FxStage MakeRecipeFx(const FloatRect& area) { return FxStage(area, area, CurrentTheme().fx); }
void TriggerRecipeFx(FxStage& fx, Float2 at) {
    fx.Burst(at, BurstColor, BurstCount, ParticleRecipe::Debris);
    fx.Shake(ShakeSeconds);
    fx.Flash(FlashColor);
}
// Owner calls fx.SetQuality(Effects()), fx.Update(chosenDelta), and fx.Clear() on reset.
void DrawRecipeFx(Painter& paint, const FxStage& fx, const FloatRect& viewArea) {
    {
        auto clip = paint.Clip(viewArea);
        auto camera = paint.Transform(fx.WorldCamera(static_cast<float>(paint.Time())).Transform());
        // Draw production world objects here, before the world effects.
        fx.DrawWorldLayer(paint);
    }
    fx.DrawOverlay(paint);
}
```
<!-- /recipe -->

Pitfalls: Pass Game::Effects() to SetQuality. Clip before applying the rotated camera. Overlay drawing stays outside the camera; Clear on scene/run reset.

Concept: `Docs/Graphics/Effects.md#effect-stage-and-particle-recipes`.
