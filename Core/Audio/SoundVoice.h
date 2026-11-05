#pragma once
#include <cstdint>

// One sound being played by the device. Stale IDs are safe: a voice's ID is never given out again, and every call on
// one that has ended does nothing. Empty (0) means no voice. Games normally hold a LiveSound, which is made of this.
enum class VoiceId : std::uint64_t {};

// How a voice begins. Everything can change while it plays, except `loop`, `startAt` and `startFrame`.
struct VoiceStart {
    float volume = 1.0f;          // 0 to 1, reached after fadeIn
    float pitch = 1.0f;           // Finite and positive; speed changes with it
    float gain = 1.0f;            // 0 to 1: the placement's gain
    float pan = 0.0f;             // -1 (left) to 1 (right)
    float muffle = 0.0f;          // 0 (clear) to 1 (behind a thick wall)
    float fadeIn = 0.0f;          // Seconds, finite and not negative
    float startAt = 0.0f;         // Seconds into the file; past the end of a loop wraps around
    std::uint64_t startFrame = 0; // On AudioSystem::ClockFrames; 0 is now. Voices given one frame start in step
    bool loop = false;
    bool owned = false; // Held by a handle: outside the effect's maxConcurrent, and never restarted to make room
    bool paused = false; // Begins held in place, until PauseVoice lets it go
};
