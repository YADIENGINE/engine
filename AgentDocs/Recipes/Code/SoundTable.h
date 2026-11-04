// [recipe:SoundTable-header]
#pragma once
#include <array>
#include "Audio/SoundRow.h"
#include "Utilities/EnumTable.h"

class Game;
struct GameSettings;
class SoundPlayer;

// One enum value per sound. A new value without a row fails to compile, and so does a misspelled one.
enum class RecipeSfx { Hit, Coin, UiMove, Count };

inline constexpr std::array RecipeSfxTable{
    SoundRow{RecipeSfx::Hit,
             {"Audio/hit1.wav", "Audio/hit2.wav"}, // One of these at random, never the same twice in a row
             {.pitch = {0.95f, 1.05f}, .cooldown = 0.03f, .label = "Hit"}},
    SoundRow{RecipeSfx::Coin, {"Audio/coin.wav"}, {.volume = 0.8f, .label = "Coin"}},
    SoundRow{RecipeSfx::UiMove, {"Audio/move.wav"}, {.whenPlaying = SoundOverlap::Skip, .label = "UI move"}},
};
static_assert(RowsInIdOrder(RecipeSfxTable));

void AddRecipeSounds(GameSettings& settings);
// What a scene does when something happens: `combo` raises the pitch of later coins.
void RecipeCollect(Game& game, int combo);
// The same call where a system was handed only the player, as an editor scene is (SceneServices::sounds).
bool RecipeHit(SoundPlayer& sounds);
// [/recipe]
