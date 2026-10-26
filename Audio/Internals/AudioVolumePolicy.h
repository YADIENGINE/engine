#pragma once
#include <cstdint>
#include "Audio/AudioSystem.hpp"

class GameOptions;
// Headless volume stepping and serialization into the engine's options; never writes a file itself.
class AudioVolumePolicy {
public:
    explicit AudioVolumePolicy(AudioVolumeSettings settings = {});
    [[nodiscard]] float Step(float volume, int steps) const;
    [[nodiscard]] static int Percent(float volume);
    [[nodiscard]] float LoadEffects(const GameOptions& options) const;
    [[nodiscard]] float LoadMusic(const GameOptions& options) const;
    [[nodiscard]] float LoadMenuMusic(const GameOptions& options) const;
    void Store(GameOptions& options, float effects, float music, float menuMusic) const;

private:
    [[nodiscard]] int NearestStep(float volume) const;
    [[nodiscard]] float Load(std::int64_t percent) const;
    AudioVolumeSettings _settings;
};
