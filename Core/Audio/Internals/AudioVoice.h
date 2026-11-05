#pragma once
#include <miniaudio.h>
#include <filesystem>
#include "Audio/AudioSystem.h"
#include "Audio/Internals/ShapeNode.h"

// One sound of one file with its stop in the graph, played again and again rather than made anew. Music and effects
// are both this. Runs on miniaudio's atomics, so nothing here waits for the audio thread.
class AudioVoice {
public:
    AudioVoice(ma_engine& engine, ma_node* bus, std::filesystem::path file, bool streamed);
    ~AudioVoice();

    void Begin(VoiceId id, const VoiceStart& start, float baseVolume);
    [[nodiscard]] VoiceId Id() const { return _id; }
    [[nodiscard]] bool Owned() const { return _owned; }

    // In use, including a fade out. Free for Begin otherwise.
    [[nodiscard]] bool Busy() const { return _id != VoiceId{}; }
    // Busy, not fading out and not at its end.
    [[nodiscard]] bool Alive() const;
    [[nodiscard]] bool Paused() const { return _paused; }

    void SetPlacement(float gain, float pan);
    void SetBaseVolume(float volume);
    void SetPitch(float pitch);
    void Fade(float volume, float seconds);
    void Muffle(float amount, float seconds);
    void Pause(bool paused);
    void Stop(float fadeSeconds);
    void SetLooping(bool loop);
    // Seconds into the file. A voice that is not busy just remembers its place.
    void Seek(float seconds);
    [[nodiscard]] float Length() const;
    [[nodiscard]] float Cursor() const;

    // Collects what happened on the audio thread: a fade finished, a hold came to rest, the end was reached.
    void Tick();

private:
    void Load();
    void End();
    void ApplyEars();

    ma_engine& _engine;
    std::filesystem::path _file;
    bool _streamed;
    ShapeNode _node;
    ma_sound _sound{};
    mutable float _knownLength = -1.0f;
    VoiceId _id{};
    float _baseVolume = 1.0f;
    float _gain = 1.0f;
    float _pan = 0.0f;
    bool _owned = false;
    bool _stopping = false;
    bool _paused = false;
    bool _soundHeld = false; // The sound itself has stopped; the shaper holding it brought it there softly
    bool _stale = false;     // Stopped part-way, with frames read ahead that a seek would not throw away

public:
    AudioVoice(const AudioVoice&) = delete;
    AudioVoice& operator=(const AudioVoice&) = delete;
};
