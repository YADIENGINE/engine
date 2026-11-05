// [recipe:SpatialSound-header]
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
// [/recipe]
