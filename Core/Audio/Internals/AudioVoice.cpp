#include "Audio/Internals/AudioVoice.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include "Audio/Internals/AudioDecoder.h"
#include "Audio/SoundSpace.h"

namespace {
constexpr float QuickStopSeconds = 0.005f;
}

AudioVoice::AudioVoice(ma_engine& engine, ma_node* bus, std::filesystem::path file, bool streamed)
    : _engine(engine)
    , _file(std::move(file))
    , _streamed(streamed)
    , _node(engine, bus, ShapeNode::Running::WhileFed) {
    Load();
}

void AudioVoice::Load() {
    ma_sound_config config = ma_sound_config_init_2(&_engine);
#ifdef _WIN32
    config.pFilePathW = _file.c_str();
#else
    config.pFilePath = _file.c_str();
#endif
    config.flags = MA_SOUND_FLAG_NO_SPATIALIZATION | (_streamed ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE);
    config.pInitialAttachment = _node.Node();
    const ma_result made = ma_sound_init_ex(&_engine, &config, &_sound);
    if (made != MA_SUCCESS)
        throw std::runtime_error("Could not load sound: " + _file.string() + ": " + ma_result_description(made));
}

AudioVoice::~AudioVoice() { ma_sound_uninit(&_sound); }

void AudioVoice::Begin(VoiceId id, const VoiceStart& start, float baseVolume) {
    if (Busy()) End();
    // miniaudio's seek leaves the frames a sound has read ahead, and they would play first (about 1.3 ms of the old
    // place, measured). A new sound has none: decoded files stay in memory, so this costs little.
    if (_stale) {
        ma_sound_uninit(&_sound);
        Load();
        _stale = false;
    }
    _id = id;
    _owned = start.owned;
    _baseVolume = baseVolume;
    _gain = start.gain;
    _pan = start.pan;
    _stopping = false;
    _paused = start.paused;
    _soundHeld = start.paused;

    const SoundEars ears = PanEars(_pan);
    const float level = _baseVolume * _gain;
    _node.Shaper().Snap({.left = ears.left * level,
                         .right = ears.right * level,
                         .volume = start.fadeIn > 0.0f ? 0.0f : start.volume,
                         .muffle = start.muffle,
                         .held = start.paused});
    if (start.fadeIn > 0.0f) _node.Shaper().FadeTo(start.volume, start.fadeIn);

    ma_sound_set_looping(&_sound, start.loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_pitch(&_sound, start.pitch);
    ma_sound_set_start_time_in_pcm_frames(&_sound, start.startFrame);
    if (start.startAt > 0.0f) Seek(start.startAt);
    if (start.paused) return;
    const bool atEnd = ma_sound_at_end(&_sound) != MA_FALSE;
    ma_sound_start(&_sound);
    if (atEnd && start.startAt > 0.0f) Seek(start.startAt); // Starting from the end goes back to the top by itself
}

bool AudioVoice::Alive() const {
    return Busy() && !_stopping && (_paused || ma_sound_at_end(&_sound) == MA_FALSE);
}

void AudioVoice::ApplyEars() {
    const SoundEars ears = PanEars(_pan);
    const float level = _baseVolume * _gain;
    _node.Shaper().SetEars(ears.left * level, ears.right * level);
}

void AudioVoice::SetPlacement(float gain, float pan) {
    _gain = gain;
    _pan = pan;
    ApplyEars();
}

void AudioVoice::SetBaseVolume(float volume) {
    _baseVolume = volume;
    ApplyEars();
}

void AudioVoice::SetPitch(float pitch) { ma_sound_set_pitch(&_sound, pitch); }
void AudioVoice::Fade(float volume, float seconds) { _node.Shaper().FadeTo(volume, seconds); }
void AudioVoice::Muffle(float amount, float seconds) { _node.Shaper().MuffleTo(amount, seconds); }

void AudioVoice::Pause(bool paused) {
    if (_paused == paused || !Busy()) return;
    _paused = paused;
    _node.Shaper().SetHeld(paused);
    if (paused || !_soundHeld) return;
    _soundHeld = false;
    ma_sound_start(&_sound);
}

void AudioVoice::Stop(float fadeSeconds) {
    if (!Busy() || _stopping) return;
    if (_soundHeld) { // Already silent, and its fade would never move
        End();
        return;
    }
    _stopping = true;
    _node.Shaper().FadeTo(0.0f, std::max(fadeSeconds, QuickStopSeconds));
}

void AudioVoice::SetLooping(bool loop) { ma_sound_set_looping(&_sound, loop ? MA_TRUE : MA_FALSE); }

void AudioVoice::Seek(float seconds) {
    const float length = Length();
    float target = seconds;
    if (length > 0.0f) target = ma_sound_is_looping(&_sound) ? std::fmod(seconds, length) : std::min(seconds, length);
    ma_sound_seek_to_second(&_sound, target);
}

float AudioVoice::Length() const {
    if (_knownLength >= 0.0f) return _knownLength;
    float seconds = 0.0f;
    if (ma_sound_get_length_in_seconds(const_cast<ma_sound*>(&_sound), &seconds) != MA_SUCCESS) seconds = 0.0f;
    // A Vorbis stream does not know its length through the way miniaudio reads it; opened from the file, it does.
    _knownLength = seconds > 0.0f ? seconds : AudioFileSeconds(_file);
    return _knownLength;
}

float AudioVoice::Cursor() const {
    float seconds = 0.0f;
    return ma_sound_get_cursor_in_seconds(&_sound, &seconds) == MA_SUCCESS ? seconds : 0.0f;
}

void AudioVoice::End() {
    _stale = ma_sound_at_end(&_sound) == MA_FALSE;
    ma_sound_stop(&_sound);
    _id = VoiceId{};
    _stopping = false;
    _paused = false;
    _soundHeld = false;
}

void AudioVoice::Tick() {
    if (!Busy()) return;
    if (_paused && !_soundHeld && _node.Shaper().HeldQuiet()) {
        ma_sound_stop(&_sound);
        _soundHeld = true;
    }
    if (_paused) {
        if (_stopping) End();
        return;
    }
    if (ma_sound_at_end(&_sound) != MA_FALSE || (_stopping && _node.Shaper().Volume() <= 0.0f)) End();
}
