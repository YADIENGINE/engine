#pragma once
#include "Audio/AudioSystem.h"

// What SoundPlayer needs from the audio device. Game gives it AudioSystem; a test gives it a fake, so that everything
// the player decides runs with no device.
class SoundOutput {
public:
    virtual ~SoundOutput() = default;
    [[nodiscard]] virtual bool IsPlaying(SoundId id) const = 0;
    virtual void Play(SoundId id, float pitch) = 0;

protected:
    SoundOutput() = default;
    SoundOutput(const SoundOutput&) = default;
    SoundOutput& operator=(const SoundOutput&) = default;
};
