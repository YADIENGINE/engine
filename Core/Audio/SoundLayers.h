#pragma once
#include <initializer_list>
#include <span>
#include <vector>
#include "Audio/LiveSound.h"
#include "Audio/SoundKey.h"

class SoundPlayer;

// One layer and the volume it should have: `layers.Mix({{Ambience::Wind, 0.7f}, {Ambience::Drip, 0.3f}}, 2.0f)`.
struct SoundLayerLevel {
    SoundKey key;
    float volume = 1.0f;
};

// Several loops at once and soft moves between them: an ambience, or a piece of music in stems. Layers are rows of the
// game's sound tables, so they reach the package like any other sound. A layer once started keeps running, silent when
// its volume is 0, so it never jumps back to its start. Fades cross at constant power: two layers trading places do not
// dip in loudness. Nothing needs updating every frame. The layers end when this object goes away, or at Stop.
class SoundLayers {
public:
    static constexpr float DefaultFade = 1.0f; // Seconds

    // `sounds` must outlive this object.
    explicit SoundLayers(SoundPlayer& sounds, float fadeSeconds = DefaultFade);

    // Starts these layers silent and sample-aligned, for files of one length that must stay in step (stems). A layer
    // started later by Set or Mix runs on its own clock, which suits an ambience.
    void Prepare(std::span<const SoundKey> keys);
    void Prepare(std::initializer_list<SoundKey> keys) { Prepare(std::span(keys.begin(), keys.size())); }
    // One layer's volume, 0 to 1. Starts the layer if it was not running. Negative seconds: the default fade.
    void Set(SoundKey key, float volume, float seconds = -1.0f);
    // Exactly this mix: the listed layers go to their volumes, all the others to 0.
    void Mix(std::initializer_list<SoundLayerLevel> levels, float seconds = -1.0f);
    // Everything down to silence and back; the layers keep their volumes and their place in the file meanwhile.
    void Silence(float seconds = -1.0f);
    void Restore(float seconds = -1.0f);
    // All layers at once, as behind a closed door.
    void SetMuffle(float amount, float seconds = -1.0f);
    // Ends every layer. Set or Mix starts them again, from the top.
    void Stop(float seconds = -1.0f);

    // The volume it was last asked for, whatever the fade has reached and with Silence left out. 0 for an unknown key.
    [[nodiscard]] float Volume(SoundKey key) const;
    [[nodiscard]] bool Silenced() const { return _silenced; }

private:
    struct Layer {
        SoundKey key;
        LiveSound sound;
        float volume = 0.0f;
    };
    [[nodiscard]] Layer* Find(SoundKey key);
    [[nodiscard]] const Layer* Find(SoundKey key) const;
    [[nodiscard]] Layer& Started(SoundKey key);
    [[nodiscard]] float Seconds(float seconds) const;
    void Apply(Layer& layer, float seconds);

    SoundPlayer* _sounds;
    float _fadeSeconds;
    float _muffle = 0.0f;
    bool _silenced = false;
    std::vector<Layer> _layers;
};
