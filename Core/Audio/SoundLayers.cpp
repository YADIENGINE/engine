#include "Audio/SoundLayers.h"
#include <algorithm>
#include <stdexcept>
#include <utility>
#include "Audio/Internals/Validation.h"
#include "Audio/SoundPlayer.h"

SoundLayers::SoundLayers(SoundPlayer& sounds, float fadeSeconds)
    : _sounds(&sounds)
    , _fadeSeconds(fadeSeconds) {
    ValidateAudioFade(fadeSeconds);
}

SoundLayers::Layer* SoundLayers::Find(SoundKey key) {
    const auto found = std::ranges::find_if(_layers, [key](const Layer& layer) { return layer.key == key; });
    return found == _layers.end() ? nullptr : &*found;
}
const SoundLayers::Layer* SoundLayers::Find(SoundKey key) const {
    const auto found = std::ranges::find_if(_layers, [key](const Layer& layer) { return layer.key == key; });
    return found == _layers.end() ? nullptr : &*found;
}

float SoundLayers::Seconds(float seconds) const { return seconds < 0.0f ? _fadeSeconds : seconds; }

SoundLayers::Layer& SoundLayers::Started(SoundKey key) {
    if (Layer* layer = Find(key)) {
        if (layer->sound.Playing()) return *layer;
        layer->sound = _sounds->Start(key, {.volume = 0.0f, .muffle = _muffle}); // A row without .loop has ended
        layer->volume = 0.0f;
        return *layer;
    }
    return _layers.emplace_back(Layer{key, _sounds->Start(key, {.volume = 0.0f, .muffle = _muffle}), 0.0f});
}

void SoundLayers::Apply(Layer& layer, float seconds) { layer.sound.SetVolume(_silenced ? 0.0f : layer.volume, seconds); }

void SoundLayers::Prepare(std::span<const SoundKey> keys) {
    for (const SoundKey key : keys)
        if (Find(key)) throw std::logic_error("A layer to prepare is already running");
    auto sounds = _sounds->StartTogether(keys, {.volume = 0.0f, .muffle = _muffle});
    for (std::size_t i = 0; i < keys.size(); ++i)
        _layers.push_back({keys[i], std::move(sounds[i]), 0.0f});
}

void SoundLayers::Set(SoundKey key, float volume, float seconds) {
    ValidateAudioVolume(volume);
    if (seconds >= 0.0f) ValidateAudioFade(seconds);
    Layer& layer = Started(key);
    layer.volume = volume;
    Apply(layer, Seconds(seconds));
}

void SoundLayers::Mix(std::initializer_list<SoundLayerLevel> levels, float seconds) {
    for (const SoundLayerLevel& level : levels)
        ValidateAudioVolume(level.volume);
    if (seconds >= 0.0f) ValidateAudioFade(seconds);
    for (const SoundLayerLevel& level : levels)
        Started(level.key).volume = level.volume;
    for (Layer& layer : _layers) {
        const bool listed =
            std::ranges::any_of(levels, [&layer](const SoundLayerLevel& level) { return level.key == layer.key; });
        if (!listed) layer.volume = 0.0f;
        Apply(layer, Seconds(seconds));
    }
}

void SoundLayers::Silence(float seconds) {
    if (seconds >= 0.0f) ValidateAudioFade(seconds);
    _silenced = true;
    for (Layer& layer : _layers)
        Apply(layer, Seconds(seconds));
}

void SoundLayers::Restore(float seconds) {
    if (seconds >= 0.0f) ValidateAudioFade(seconds);
    _silenced = false;
    for (Layer& layer : _layers)
        Apply(layer, Seconds(seconds));
}

void SoundLayers::SetMuffle(float amount, float seconds) {
    ValidateAudioMuffle(amount);
    if (seconds >= 0.0f) ValidateAudioFade(seconds);
    _muffle = amount;
    for (Layer& layer : _layers)
        layer.sound.SetMuffle(amount, Seconds(seconds));
}

void SoundLayers::Stop(float seconds) {
    if (seconds >= 0.0f) ValidateAudioFade(seconds);
    for (Layer& layer : _layers)
        layer.sound.Stop(Seconds(seconds));
    _layers.clear();
}

float SoundLayers::Volume(SoundKey key) const {
    const Layer* layer = Find(key);
    return layer ? layer->volume : 0.0f;
}
