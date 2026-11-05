# Spatial sound recipe

Sounds that come from a place on screen, loops a view keeps changing, and ambience layers that cross into each other. The view follows the simulation's state every frame; the simulation knows nothing of sound.

APIs: Core/Audio/SoundPlayer.h, Core/Audio/LiveSound.h, Core/Audio/SoundLayers.h, Core/Audio/SoundSpace.h, Core/Audio/SoundRow.h. Concepts: `Docs/Audio.md#sounds-in-the-world`.

<!-- recipe:SpatialSound-header -->
```cpp
#pragma once
#include <array>
#include "Audio/LiveSound.h"
#include "Audio/SoundLayers.h"
#include "Audio/SoundRow.h"
#include "Camera/CameraView.h"
#include "Utilities/EnumTable.h"

class SoundPlayer;
struct GameSettings;

// Loops and layers are rows like any other sound, so the package keeps their files (Docs/Release.md).
enum class RecipeNoise { Hum, Heart, Wind, Rain, Count };

inline constexpr std::array RecipeNoiseTable{
    SoundRow{RecipeNoise::Hum,
             {"Audio/hum.ogg"},
             {.loop = true, .falloff = {.fullWithin = 100.0f, .silentBeyond = 900.0f}, .label = "Hum"}},
    SoundRow{RecipeNoise::Heart, {"Audio/heart.ogg"}, {.loop = true, .bus = SoundBus::Ui, .label = "Heart"}},
    SoundRow{RecipeNoise::Wind, {"Audio/wind.ogg"}, {.loop = true, .streamed = true, .label = "Wind"}},
    SoundRow{RecipeNoise::Rain, {"Audio/rain.ogg"}, {.loop = true, .streamed = true, .label = "Rain"}},
};
static_assert(RowsInIdOrder(RecipeNoiseTable));

void AddRecipeNoises(GameSettings& settings);

// What the view reads from the simulation's state each frame. The sounds follow it; the simulation knows none of this.
struct RecipeHauntState {
    Float2 player{};
    Float2 ghost{};
    bool indoors = false;
};

// A view's sounds: a hum on a moving thing, a heartbeat that quickens as it nears, an ambience that changes with the place.
class RecipeHauntSounds {
public:
    explicit RecipeHauntSounds(SoundPlayer& sounds);
    // Every frame, after the simulation advanced. Nothing here needs the events of N1: it follows the state.
    void Update(const RecipeHauntState& state, const CameraView& view);
    [[nodiscard]] float BeatsPerSecond() const { return _beats; }

private:
    SoundPlayer& _sounds;
    LiveSound _hum;
    LiveSound _heart;
    SoundLayers _ambience;
    bool _indoors = false;
    float _beats = 1.0f;
};
```
<!-- /recipe -->

<!-- recipe:SpatialSound -->
```cpp
#include "AgentDocs/Recipes/Code/SpatialSound.h"
#include <algorithm>
#include <cmath>
#include "Audio/SoundPlayer.h"
#include "Runtime/GameSettings.h"

void AddRecipeNoises(GameSettings& settings) { settings.sounds = RecipeNoiseTable; }

RecipeHauntSounds::RecipeHauntSounds(SoundPlayer& sounds)
    : _sounds(sounds)
    , _hum(sounds.Start(RecipeNoise::Hum, {.fadeIn = 1.0f}))
    , _heart(sounds.Start(RecipeNoise::Heart, {.volume = 0.0f}))
    , _ambience(sounds, 2.0f) {
    _ambience.Set(RecipeNoise::Wind, 0.8f);
}

void RecipeHauntSounds::Update(const RecipeHauntState& state, const CameraView& view) {
    // The listener is the camera: what is right of the screen's middle is heard on the right.
    _sounds.SetListener({.position = view.target, .rotation = view.rotation, .panWidth = view.VisibleArea().width / 2.0f});
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
```
<!-- /recipe -->

A one-shot at a place needs no handle: `sounds.Play(Sfx::Door, {.at = doorPosition})`. Stems of one piece of music that must stay in step: `layers.Prepare({Music::Drums, Music::Bass})`, then `Set` each.

Pitfalls: a row with `.loop` cannot go through `Play`, which would leave it running forever; use `Start` and keep the handle. A dropped `LiveSound` fades out and stops, so keep it as a member, not a local. Without a `SetListener` call the listener stands at the origin. A long file belongs in a row with `.streamed = true`, or it is held whole in memory. MP3 adds padding at its ends, so loop OGG, FLAC or WAV. Every loop and layer is a table row, which is what keeps its file in the release package; a file loaded by hand is left out. Never start or change a sound inside a simulation step: read the state in the view, after `Advance`.
