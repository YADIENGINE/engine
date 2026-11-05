#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "Audio/SoundBus.h"
#include "Audio/SoundVoice.h"

class GameOptions;

enum class SoundId : std::size_t {};
enum class MusicId : std::size_t {};
enum class MusicVolumeGroup { Default, Menu };
enum class EffectOverflow { RestartNext, Skip };

struct EffectSettings {
    std::size_t maxConcurrent = 4;
    float volume = 1.0f;
    EffectOverflow overflow = EffectOverflow::RestartNext;
    SoundBus bus = SoundBus::World;
    bool streamed = false; // Read from disk while it plays, not loaded into memory: for long files
};
// `name` is optional and only labels the asset in tools such as the editor; playback ignores it.
struct EffectAsset {
    SoundId id;
    std::filesystem::path file;
    EffectSettings settings{};
    std::string name;
};
struct MusicAsset {
    MusicId id;
    std::filesystem::path file;
    MusicVolumeGroup volumeGroup = MusicVolumeGroup::Default;
    std::string name;
};
// IDs belong to the project; paths are relative to its asset root.
struct AudioCatalog {
    std::vector<EffectAsset> effects;
    std::vector<MusicAsset> music;
};
struct AudioVolumeSettings {
    int steps = 4;
};
// How a track starts. With a fade, the track playing now crosses into this one.
struct MusicPlay {
    bool loop = true;
    float fadeSeconds = 0.0f;
};
// Device plays through the sound card. Offline opens none: Render hands out the mix on demand, for tests and tools.
enum class AudioOutput { Device, Offline };

// One audio engine, on the device or offline. Use from the creating thread; Game owns and updates its instance.
class AudioSystem final {
public:
    explicit AudioSystem(std::filesystem::path assetRoot, AudioVolumeSettings volumeSettings = {},
                         AudioOutput output = AudioOutput::Device);
    ~AudioSystem();

    // Checks IDs, relative paths and settings without filesystem IO or an audio device.
    // Invalid catalogs throw std::invalid_argument. BeginLoading also performs this check.
    static void ValidateCatalog(const AudioCatalog& catalog);
    // Starts one loading attempt. Invalid catalogs leave the system unstarted.
    void BeginLoading(AudioCatalog catalog);
    // False while loading, true when ready. Before BeginLoading: std::logic_error.
    // Loading failures are retained and rethrown on every subsequent call.
    [[nodiscard]] bool FinishLoading();

    // Resource operations and playback require successful FinishLoading.
    // Replacement is transactional: loading failure preserves the existing resource.
    void LoadEffect(SoundId id, const std::filesystem::path& file, EffectSettings settings = {});
    void UnloadEffect(SoundId id);
    // False until ready, or when the ID is absent.
    [[nodiscard]] bool HasEffect(SoundId id) const;
    // Pitch must be finite and positive; changing pitch also changes playback speed.
    void PlayEffect(SoundId id, float pitch = 1.0f);
    // Skips the request if any copy of this effect is playing.
    void PlayEffectIfIdle(SoundId id, float pitch = 1.0f);
    void StopEffect(SoundId id);
    void SetEffectVolume(SoundId id, float volume);
    // Whether any copy is playing; false until ready, or when the ID is absent.
    [[nodiscard]] bool IsEffectPlaying(SoundId id) const;

    // One play of an effect that can still be changed. Empty when EffectOverflow::Skip dropped it. Invalid values throw
    // std::invalid_argument. Every change glides, and a call on a voice that has ended does nothing. SoundPlayer and
    // LiveSound are built on these, and a game's sound tables normally reach them through those.
    [[nodiscard]] VoiceId StartVoice(SoundId id, const VoiceStart& start);
    // True until it has ended or StopVoice was called; a paused voice is alive.
    [[nodiscard]] bool IsVoiceAlive(VoiceId voice) const;
    void SetVoicePlacement(VoiceId voice, float gain, float pan);
    void SetVoicePitch(VoiceId voice, float pitch);
    void FadeVoice(VoiceId voice, float volume, float seconds);
    void MuffleVoice(VoiceId voice, float amount, float seconds);
    void PauseVoice(VoiceId voice, bool paused);
    void StopVoice(VoiceId voice, float fadeSeconds);
    // A bus's own level and muffling, under the volume options.
    void FadeBus(SoundBus bus, float volume, float seconds);
    void MuffleBus(SoundBus bus, float amount, float seconds);
    // The mix's clock, in frames at SampleRate. 0 until ready.
    [[nodiscard]] std::uint64_t ClockFrames() const;
    [[nodiscard]] int SampleRate() const;

    void LoadMusic(MusicId id, const std::filesystem::path& file, MusicVolumeGroup group = MusicVolumeGroup::Default);
    void UnloadMusic(MusicId id);
    [[nodiscard]] bool HasMusic(MusicId id) const;
    // Starts this track at pitch 1 and ends the previous one, at once or across MusicPlay::fadeSeconds.
    void PlayMusic(MusicId id, const MusicPlay& play = {});
    void SetMusicPitch(float pitch);
    [[nodiscard]] float MusicPitch() const;
    // Includes paused or naturally ended tracks; StopMusic clears the selection.
    [[nodiscard]] std::optional<MusicId> CurrentMusic() const;
    // Safe no-ops without a current track, including during loading.
    void StopMusic(float fadeSeconds = 0.0f);
    void PauseMusic();
    void ResumeMusic();
    [[nodiscard]] bool IsMusicPlaying() const;
    // The current track's length and playback position in seconds; 0 without a current track.
    [[nodiscard]] float MusicLength() const;
    [[nodiscard]] float MusicPosition() const;
    // Seconds must be finite and non-negative; a time past the end goes to the end. Without a current track, a no-op.
    void SeekMusic(float seconds);
    // Changes the current track's looping; a no-op without a current track.
    void SetMusicLooping(bool loop);
    // Collects what has ended. Game already calls this every frame; playback itself does not depend on it.
    void Update();

    // AudioOutput::Offline only (std::logic_error otherwise): the next frames of the mix, as interleaved stereo. A span
    // of an odd size throws std::invalid_argument.
    void Render(std::span<float> stereo);
    // Writes what the device plays, for this many seconds, to a 32-bit float stereo WAV: the way to check by numbers
    // what nobody can hear. One at a time; a new one ends the last. Throws std::runtime_error if the file cannot be made.
    void StartCapture(const std::filesystem::path& wavFile, float seconds);
    [[nodiscard]] bool IsCapturing() const;

    // Volume setters accept finite values in [0, 1]; they may be used before/during loading.
    void SetMasterVolume(float volume);
    void SetEffectVolume(float volume);
    void SetMusicVolume(float volume, MusicVolumeGroup group = MusicVolumeGroup::Default);
    void SetMuted(bool muted);
    bool AdjustEffectVolume(int steps);
    bool AdjustMusicVolume(int steps, MusicVolumeGroup group = MusicVolumeGroup::Default);
    [[nodiscard]] int EffectVolumePercent() const;
    [[nodiscard]] int MusicVolumePercent(MusicVolumeGroup group = MusicVolumeGroup::Default) const;
    [[nodiscard]] float MasterVolume() const;
    [[nodiscard]] float EffectVolume() const;
    [[nodiscard]] float MusicVolume(MusicVolumeGroup group = MusicVolumeGroup::Default) const;
    [[nodiscard]] bool IsMuted() const;

private:
    friend class Game;
    friend void LoadMusicFile(AudioSystem& audio, MusicId id, const std::filesystem::path& file);
    void LoadVolumeSettings(const GameOptions& options);
    void StoreVolumeSettings(GameOptions& options) const;

    class State;
    std::unique_ptr<State> _state;

public:
    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;
    AudioSystem(AudioSystem&&) = delete;
    AudioSystem& operator=(AudioSystem&&) = delete;
};
