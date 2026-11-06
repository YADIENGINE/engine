#pragma once
#include "Fx/FxStage.h"
#include "Fx/Particles/ParticleEmitter.h"

// A moving object with a trail and a torch that burns; each owns its streams, the stage draws them.
struct RecipeRunner {
    Float2 position{};
    Float2 velocity{};
    ParticleEmitter trail;
};
struct RecipeTorch {
    Float2 position{};
    ParticleEmitter flame;
    ParticleEmitter embers;
    ParticleEmitter smoke;
};

[[nodiscard]] RecipeRunner MakeRecipeRunner(Float2 position);
[[nodiscard]] RecipeTorch MakeRecipeTorch(Float2 position);
void UpdateRecipeStreams(FxStage& fx, RecipeRunner& runner, RecipeTorch& torch, float gameSeconds);
void TeleportRecipeRunner(RecipeRunner& runner, Float2 to);
