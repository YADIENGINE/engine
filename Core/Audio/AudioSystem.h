#pragma once
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class GameOptions;

enum class SoundId : std::size_t {};
enum class MusicId : std::size_t {};
enum class MusicVolumeGroup { Default, Menu };
enum class EffectOverflow { RestartNext, Skip };

struct EffectSettings {
    std::size_t maxConcurrent = 4;
    float volume = 1.0f;
    EffectOverflow overflow = EffectOverflow::RestartNext;
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

// One owned audio device. Use from the creating thread; Game owns and updates its instance.
class AudioSystem final {
public:
    explicit AudioSystem(std::filesystem::path assetRoot, AudioVolumeSettings volumeSettings = {});
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

    void LoadMusic(MusicId id, const std::filesystem::path& file, MusicVolumeGroup group = MusicVolumeGroup::Default);
    void UnloadMusic(MusicId id);
    [[nodiscard]] bool HasMusic(MusicId id) const;
    // Stops the previous track and starts this one at pitch 1.
    void PlayMusic(MusicId id, bool loop = true);
    void SetMusicPitch(float pitch);
    [[nodiscard]] float MusicPitch() const;
    // Includes paused or naturally ended tracks; StopMusic clears the selection.
    [[nodiscard]] std::optional<MusicId> CurrentMusic() const;
    // Safe no-ops without a current track, including during loading.
    void StopMusic();
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
    // Refills streaming buffers each frame. Game already calls this.
    void Update();

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
