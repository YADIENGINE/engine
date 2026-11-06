// [recipe:SpatialSound]
#include "AgentDocs/Recipes/Code/SpatialSound.h"
#include <algorithm>
#include <cmath>
#include "Audio/SoundPlayer.h"
#include "Runtime/GameSettings.h"

void AddRecipeNoises(GameSettings& settings) { settings.sounds = RecipeNoiseTable; }

RecipeHauntSounds::RecipeHauntSounds(SoundPlayer& sounds)
    : _hum(sounds.Start(RecipeNoise::Hum, {.fadeIn = 1.0f}))
    , _heart(sounds.Start(RecipeNoise::Heart, {.volume = 0.0f}))
    , _ambience(sounds, 2.0f) {
    _ambience.Set(RecipeNoise::Wind, 0.8f);
}

void RecipeHauntSounds::Update(const RecipeHauntState& state) {
    // Heard from the camera: the scene's Stage puts a listener on each of its views, so what is right of the screen's
    // middle is heard on the right. A sound placed in the world only says where it is.
    _hum.SetPosition(state.ghost);

    const float distance = std::hypot(state.ghost.x - state.player.x, state.ghost.y - state.player.y);
    const float closeness = std::clamp(1.0f - distance / 600.0f, 0.0f, 1.0f);
    _beats = 0.9f + 1.4f * closeness; // The file is one beat long, so pitch is beats per second
    _heart.SetPitch(_beats);
    _heart.SetVolume(closeness, 0.2f);

    if (state.indoors != _indoors) {
        _indoors = state.indoors;
        _ambience.Mix({{_indoors ? RecipeNoise::Rain : RecipeNoise::Wind, 0.8f}});
        _ambience.SetMuffle(_indoors ? 0.8f : 0.0f); // Inside, the rain is behind the walls
    }
}
// [/recipe]
