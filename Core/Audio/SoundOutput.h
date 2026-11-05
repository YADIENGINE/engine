#pragma once
#include <cstdint>
#include "Audio/AudioSystem.h"
#include "Audio/SoundBus.h"
#include "Audio/SoundVoice.h"

// What SoundPlayer needs from the audio device. Game gives it AudioSystem; a test gives it a fake, so that everything
// the player decides runs with no device. Calls on a voice that has ended do nothing.
class SoundOutput {
public:
    virtual ~SoundOutput() = default;
    [[nodiscard]] virtual bool IsPlaying(SoundId id) const = 0;
    // Empty when the effect's overflow rule dropped the play.
    [[nodiscard]] virtual VoiceId StartVoice(SoundId id, const VoiceStart& start) = 0;
    [[nodiscard]] virtual bool IsVoiceAlive(VoiceId voice) const = 0;
    virtual void SetVoicePlacement(VoiceId voice, float gain, float pan) = 0;
    virtual void SetVoicePitch(VoiceId voice, float pitch) = 0;
    virtual void FadeVoice(VoiceId voice, float volume, float seconds) = 0;
    virtual void MuffleVoice(VoiceId voice, float amount, float seconds) = 0;
    virtual void PauseVoice(VoiceId voice, bool paused) = 0;
    virtual void StopVoice(VoiceId voice, float fadeSeconds) = 0;
    virtual void FadeBus(SoundBus bus, float volume, float seconds) = 0;
    virtual void MuffleBus(SoundBus bus, float amount, float seconds) = 0;
    [[nodiscard]] virtual std::uint64_t ClockFrames() const = 0;

protected:
    SoundOutput() = default;
    SoundOutput(const SoundOutput&) = default;
    SoundOutput& operator=(const SoundOutput&) = default;
};
