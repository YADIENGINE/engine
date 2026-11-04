# Sound table recipe

Give each sound of a game a typed name, with its files, volume, pitch range, cooldown and overlap rule in one table. A misspelled or missing sound fails to compile.

APIs: Core/Audio/SoundRow.h, Core/Audio/SoundTables.h, Core/Audio/SoundPlayer.h, Core/Audio/SoundKey.h, Core/Runtime/Game.h. Concepts: `Docs/Audio.md#sound-tables`.

<!-- recipe:SoundTable-header -->
```cpp
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
```
<!-- /recipe -->

<!-- recipe:SoundTable -->
```cpp
#include "AgentDocs/Recipes/Code/SoundTable.h"
#include "Runtime/Game.h"

void AddRecipeSounds(GameSettings& settings) { settings.sounds = RecipeSfxTable; }

void RecipeCollect(Game& game, int combo) {
    game.Sounds().Play(RecipeSfx::Coin, 1.0f + 0.05f * static_cast<float>(combo));
}

bool RecipeHit(SoundPlayer& sounds) { return sounds.Play(RecipeSfx::Hit); }
```
<!-- /recipe -->

A second table for another enum: `settings.sounds = {RecipeSfxTable, MyUiTable};`. A system that stores a sound without knowing the enum keeps a `SoundKey` and checks it at startup with `Has`.

Pitfalls: Put `static_assert(RowsInIdOrder(table))` beside each table, or rows out of order are found only at startup. A table must be in `GameSettings::sounds` or playing its enum throws; that is the one mistake the compiler cannot see. `Play` returning false is normal: a cooldown or `SoundOverlap::Skip` stopped it. `SoundOverlap::Copies` counts per file. After changing a table, build again before trusting `python Engine audio catalog`. A simulation never plays a sound: it lists events and the view plays them. Music stays in `GameSettings::audio`.
