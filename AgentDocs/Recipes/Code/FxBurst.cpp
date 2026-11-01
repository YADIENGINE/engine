// [recipe:FxBurst]
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
// [/recipe]
