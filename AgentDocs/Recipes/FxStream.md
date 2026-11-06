Leave a trail behind a moving object and keep a flame, embers and smoke going at a fixed place.

APIs: Core/Fx/Particles/ParticleEmitter.h, Core/Fx/FxStage.h, Core/Graphics/Style/Theme.h.

Each object owns its `ParticleEmitter`s (values, like tweens) and calls `fx.Emit` every frame; the stage draws them with
its other particles. For something that happens once, use a burst ([FxBurst](FxBurst.md)).

<!-- recipe:FxStream -->
```cpp
#include "AgentDocs/Recipes/Code/FxStream.h"
#include "Graphics/Style/Theme.h"

namespace {
constexpr Color4 TrailColor = Rgb(0x40e0ff);
constexpr Color4 TorchColor = Rgb(0xffffff); // Smoke multiplies it; flame and embers keep their own colours
} // namespace

RecipeRunner MakeRecipeRunner(Float2 position) {
    return {.position = position, .trail = ParticleEmitter(CurrentTheme().fx.particles.trail)};
}
RecipeTorch MakeRecipeTorch(Float2 position) {
    const auto& particles = CurrentTheme().fx.particles;
    return {.position = position,
            .flame = ParticleEmitter(particles.flame),
            .embers = ParticleEmitter(particles.embers),
            .smoke = ParticleEmitter(particles.smoke)};
}
// Each frame, on game time, after moving: the emitter fills the way since the last frame.
void UpdateRecipeStreams(FxStage& fx, RecipeRunner& runner, RecipeTorch& torch, float gameSeconds) {
    runner.position += runner.velocity * gameSeconds;
    fx.Emit(runner.trail, runner.position, TrailColor, gameSeconds);
    fx.Emit(torch.flame, torch.position, TorchColor, gameSeconds);
    fx.Emit(torch.embers, torch.position, TorchColor, gameSeconds);
    fx.Emit(torch.smoke, torch.position - Float2{0.0f, 12.0f}, TorchColor, gameSeconds);
}
// A jump is not travel: without Jump the next frame would draw a line of trail across the screen.
void TeleportRecipeRunner(RecipeRunner& runner, Float2 to) {
    runner.position = to;
    runner.trail.Jump();
}
```
<!-- /recipe -->

Pitfalls: Call Emit every frame with that frame's seconds, after moving. Call Jump after a teleport or respawn, and
when a stream turned off comes back on: otherwise the way travelled meanwhile is filled as one line. Do not keep an emitter in the stage or by name: it belongs to its object. Flame and embers ignore
the colour given (their own colours); smoke and the trail take it. Rain and snow over the whole view are a DriftField
([Weather](Weather.md)).

Concept: `Docs/Graphics/Effects.md#streams`.
