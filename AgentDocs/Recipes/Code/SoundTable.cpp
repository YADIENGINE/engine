// [recipe:SoundTable]
#include "AgentDocs/Recipes/Code/SoundTable.h"
#include "Runtime/Game.h"

void AddRecipeSounds(GameSettings& settings) { settings.sounds = RecipeSfxTable; }

void RecipeCollect(Game& game, int combo) {
    game.Sounds().Play(RecipeSfx::Coin, 1.0f + 0.05f * static_cast<float>(combo));
}

bool RecipeHit(SoundPlayer& sounds) { return sounds.Play(RecipeSfx::Hit); }
// [/recipe]
