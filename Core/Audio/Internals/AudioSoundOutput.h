#pragma once
#include "Audio/SoundOutput.h"

// All that lies between SoundPlayer and the device. In the Engine target; the player itself builds headlessly.
class AudioSoundOutput final : public SoundOutput {
public:
    explicit AudioSoundOutput(AudioSystem& audio)
        : _audio(audio) {}
    [[nodiscard]] bool IsPlaying(SoundId id) const override { return _audio.IsEffectPlaying(id); }
    void Play(SoundId id, float pitch) override { _audio.PlayEffect(id, pitch); }

private:
    AudioSystem& _audio;
};
