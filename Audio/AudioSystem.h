#pragma once
#include <raylib.h>
#include <atomic>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <optional>
#include <thread>
#include <vector>
#include "Audio/AudioCatalogDefinition.h"
#include "Audio/AudioVolumePolicy.h"

// Owns raylib's process-wide audio device. Loads the project's supplied catalog into numbered slots.
// All file paths are relative to the explicit asset root.
class AudioSystem {
public:
    static constexpr std::size_t DefaultVoices = 4; // Bir efektin varsayılan eş zamanlı kopya sayısı

    explicit AudioSystem(std::filesystem::path assetRoot, AudioVolumeSettings volumeSettings = {});
    ~AudioSystem();
    // Ses cihazını açar ve kataloğu arka planda yükler. FinishLoading true dönene kadar ses çalınmaz ve yüklenmez;
    // ses seviyeleri saklanır, yükleme bitince uygulanır.
    void BeginLoading(AudioCatalog catalog);
    // Yükleme bittiyse yüklenenleri devralır ve true döner (sonraki çağrılarda da). Yükleme hatası burada fırlar.
    [[nodiscard]] bool FinishLoading();
    bool AdjustEffectVolume(int steps);
    bool AdjustMusicVolume(int steps, MusicVolumeGroup group = MusicVolumeGroup::Default);
    [[nodiscard]] int EffectVolumePercent() const;
    [[nodiscard]] int MusicVolumePercent(MusicVolumeGroup group = MusicVolumeGroup::Default) const;

    // 'id' yuvasına dosya yükler; yuva doluysa önce boşaltılır. Aynı efektin en fazla 'maxConcurrent' kopyası
    // üst üste çalar; hepsi doluysa PlayEffect sıradaki kopyayı baştan başlatır.
    void LoadEffect(SoundId id, const std::filesystem::path& file, std::size_t maxConcurrent = DefaultVoices);
    void UnloadEffect(SoundId id);
    [[nodiscard]] bool HasEffect(SoundId id) const;
    // 'pitch': 1 normal; büyüdükçe ince ve hızlı çalar.
    void PlayEffect(SoundId id, float pitch = 1.0f);
    // Efekt zaten çalıyorsa yeni isteği atlar; çalan sesi kesmez veya sıraya almaz.
    void PlayEffectIfIdle(SoundId id, float pitch = 1.0f);
    void StopEffect(SoundId id);
    // Tek efektin kendi ses seviyesi (0..1); genel efekt seviyesiyle çarpılır.
    void SetEffectVolume(SoundId id, float volume);

    // 'id' yuvasına müzik yükler; yuva doluysa önce boşaltılır.
    void LoadMusic(MusicId id, const std::filesystem::path& file, MusicVolumeGroup group = MusicVolumeGroup::Default);
    void UnloadMusic(MusicId id);
    [[nodiscard]] bool HasMusic(MusicId id) const;
    // Çalan müzik durur; yeni müzik normal hızda (pitch 1) başlar.
    void PlayMusic(MusicId id, bool loop = true);
    // Çalan müziğin hızı (raylib: hız ve perde birlikte değişir; 1 normal).
    void SetMusicPitch(float pitch);
    [[nodiscard]] float MusicPitch() const { return _musicPitch; }
    // Çalan (ya da duraklatılmış) parça; StopMusic sonrası yok.
    [[nodiscard]] std::optional<MusicId> CurrentMusic() const { return _currentMusic; }
    void StopMusic();
    void PauseMusic();
    void ResumeMusic();
    [[nodiscard]] bool IsMusicPlaying() const;
    // Müzik akışını doldurur; her frame çağrılır.
    void Update();

    void SetMasterVolume(float volume);
    void SetEffectVolume(float volume);
    void SetMusicVolume(float volume, MusicVolumeGroup group = MusicVolumeGroup::Default);
    void SetMuted(bool muted);
    [[nodiscard]] float MasterVolume() const { return _masterVolume; }
    [[nodiscard]] float EffectVolume() const { return _effectVolume; }
    [[nodiscard]] float MusicVolume(MusicVolumeGroup group = MusicVolumeGroup::Default) const {
        return group == MusicVolumeGroup::Menu ? _menuMusicVolume : _musicVolume;
    }
    [[nodiscard]] bool IsMuted() const { return _muted; }
    [[nodiscard]] const std::filesystem::path& Root() const { return _root; }

private:
    // Ses seviyeleri engine tercihleridir; Game açılışta yükler, her kayıt noktasında yazar.
    friend class Game;
    void LoadVolumeSettings(const GameOptions& options);
    void StoreVolumeSettings(GameOptions& options) const;

    struct MusicSlot {
        ::Music stream{};
        MusicVolumeGroup volumeGroup = MusicVolumeGroup::Default;
    };
    struct Effect {
        std::vector<Sound> voices; // İlk ses örneğin sahibidir; diğerleri onun alias'ları
        std::size_t nextVoice = 0;
        float volume = 1.0f;
    };

    [[nodiscard]] std::filesystem::path Resolve(const std::filesystem::path& file) const;
    [[nodiscard]] std::vector<Sound> LoadVoices(const std::filesystem::path& file, std::size_t maxConcurrent) const;
    [[nodiscard]] ::Music OpenMusic(const std::filesystem::path& file) const;
    void RequireLoaded() const;
    // Worker: cihazı açar, kataloğu _loadedEffects / _loadedMusic'e yükler.
    void LoadInBackground(const AudioCatalog& catalog);
    // Ana thread: worker'ı bekler, yüklenenleri yuvalara alır ve ses seviyelerini uygular.
    void AdoptLoaded();
    [[nodiscard]] Effect& GetEffect(SoundId id);
    [[nodiscard]] ::Music& GetMusic(MusicId id);
    void ApplyEffectVolume(Effect& effect) const;
    void ApplyMasterVolume() const;
    // Yüklenen her şeyi bırakır ve ses cihazını kapatır.
    void Release();
    static void ReleaseEffect(Effect& effect);

    std::filesystem::path _root;
    AudioVolumePolicy _volumePolicy;
    std::vector<Effect> _effects;
    std::vector<MusicSlot> _music;
    std::optional<MusicId> _currentMusic;
    float _musicPitch = 1.0f;
    float _masterVolume = 1.0f;
    float _effectVolume = 1.0f;
    float _musicVolume = 1.0f;
    float _menuMusicVolume = 1.0f;
    bool _muted = false;

    // Arka plan yüklemesi: worker çalışırken raylib ses API'sine ve _loaded* tamponlarına yalnız worker dokunur, ana
    // thread join'den sonra devralır (AdoptLoaded). Worker ses seviyelerini ve yuvaları okumaz.
    std::thread _loader;
    std::atomic<bool> _loaderDone{false};
    std::atomic<bool> _cancelLoading{false};
    std::exception_ptr _loadError;
    std::vector<Effect> _loadedEffects;
    std::vector<MusicSlot> _loadedMusic;
    bool _loading = false; // Ana thread'in bayrağı: BeginLoading'den AdoptLoaded'a kadar

public:
    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;
    AudioSystem(AudioSystem&&) = delete;
    AudioSystem& operator=(AudioSystem&&) = delete;
};
