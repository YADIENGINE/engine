// [recipe:FxStream]
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
// [/recipe]
