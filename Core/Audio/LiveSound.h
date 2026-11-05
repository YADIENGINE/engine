#pragma once
#include <memory>
#include "Audio/SoundKey.h"
#include "Audio/SoundVoice.h"
#include "Utilities/Geometry/Float2.h"

class LiveVoices;

// A sound that is playing and that its owner keeps changing: a loop tied to something on screen, a heartbeat that
// speeds up. SoundPlayer::Start makes one. Every change glides, however often it comes, so a view can call these every
// frame with whatever it computed from the simulation's state. Fades run on the audio thread in real time: a hitstop,
// slow motion or a long frame does not stretch them.
//
// The handle owns the sound: when it goes away the sound fades out and stops. It can outlive the player, and the sound
// can end first (a row without `.loop`); every call then does nothing. Invalid values throw std::invalid_argument.
class LiveSound {
public:
    static constexpr float DefaultStopFade = 0.05f; // Seconds: long enough not to click

    LiveSound() = default;
    ~LiveSound();
    LiveSound(LiveSound&& other) noexcept;
    LiveSound& operator=(LiveSound&& other) noexcept;

    // True from Start until Stop or the moment the sound has ended.
    [[nodiscard]] bool Playing() const;
    [[nodiscard]] explicit operator bool() const { return Playing(); }

    // 0 to 1, on top of the row's volume. `seconds` is the length of the fade; 0 still glides over a few milliseconds.
    void SetVolume(float volume, float seconds = 0.0f);
    // Finite and positive, on top of the pitch picked when it started. Speed changes with it.
    void SetPitch(float pitch);
    // Places the sound in the world: pan and gain now follow the listeners, without another call when they move.
    void SetPosition(Float2 position);
    // Takes it out of the world again: -1 (left) to 1 (right), with no falloff.
    void SetPan(float pan);
    // 0 (clear) to 1 (behind a thick wall): takes the high frequencies away.
    void SetMuffle(float amount, float seconds = 0.0f);
    // Fades out and ends; the handle is then empty.
    void Stop(float fadeSeconds = DefaultStopFade);

private:
    friend class SoundPlayer;
    LiveSound(std::weak_ptr<LiveVoices> owner, VoiceId voice);
    [[nodiscard]] std::shared_ptr<LiveVoices> Owner() const;

    std::weak_ptr<LiveVoices> _owner;
    VoiceId _voice{};

public:
    LiveSound(const LiveSound&) = delete;
    LiveSound& operator=(const LiveSound&) = delete;
};
