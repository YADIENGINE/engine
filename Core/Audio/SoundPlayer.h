#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <typeinfo>
#include <vector>
#include "Audio/LiveSound.h"
#include "Audio/SoundBus.h"
#include "Audio/SoundKey.h"
#include "Audio/SoundOutput.h"
#include "Audio/SoundSpace.h"
#include "Audio/SoundTables.h"
#include "Utilities/Random/Rng.h"

class LiveVoices;

// How one play differs from what the row says. Everything is optional: `sounds.Play(Sfx::Hit, {.at = enemy})`.
struct SoundPlay {
    float volume = 1.0f;         // 0 to 1, on top of the row's
    float pitch = 1.0f;          // Finite and positive, on top of the one picked from the row's range
    std::optional<Float2> at{};  // In the world: pan and falloff come from the listeners, and `pan` is ignored
    float pan = 0.0f;            // -1 (left) to 1 (right), for a sound that is not in the world
    float muffle = 0.0f;         // 0 (clear) to 1 (behind a thick wall)
    float fadeIn = 0.0f;         // Seconds
    float startAt = 0.0f;        // Seconds into the file. Loops that are all alike sound better from different places
};

// Plays the rows of the game's sound tables: `game.Sounds().Play(Sfx::Hit)` for something that happens,
// `game.Sounds().Start(Sfx::Engine)` for something that goes on and changes. It picks the file and the pitch and keeps
// the cooldowns; only the last step reaches the device. Use it from the thread that owns the audio. Not for a
// simulation: a sound is something a view does about what the simulation said.
class SoundPlayer {
public:
    static constexpr std::uint64_t DefaultSeed = 0x536F756E64; // Fixed: nobody can hear that two runs pick alike

    SoundPlayer(const SoundTables& tables, SoundOutput& output, std::uint64_t seed = DefaultSeed);
    ~SoundPlayer();

    // Whether the key names a row of a registered table. It asks the tables, not the device, so a system can check its
    // keys at startup, before audio has loaded. An empty key: false.
    [[nodiscard]] bool Has(SoundKey key) const;
    // Whether any file of the row is playing now; false for a key Has rejects. For "never overlap itself" whatever the
    // row says: `if (!sounds.IsPlaying(key)) sounds.Play(key);`.
    [[nodiscard]] bool IsPlaying(SoundKey key) const;
    // Plays the row and forgets it. False when the row's cooldown or SoundOverlap::Skip stopped it. Throws
    // std::out_of_range for a key Has rejects (a table that was never registered, an empty key, Enum::Count),
    // std::invalid_argument for a value out of its range, and std::logic_error before audio has loaded or for a row
    // with `.loop`, which nothing could ever stop.
    bool Play(SoundKey key, const SoundPlay& play = {});
    // Starts the row and hands back the sound, to change while it plays and to stop. Cooldown and overlap rules do not
    // apply: the handle says how long it lasts. Throws as Play does, except for `.loop`.
    [[nodiscard]] LiveSound Start(SoundKey key, const SoundPlay& play = {});
    // Starts rows sample-aligned, for files of one length that must stay in step. The handles are in the keys' order.
    [[nodiscard]] std::vector<LiveSound> StartTogether(std::span<const SoundKey> keys, const SoundPlay& play = {});

    // Who hears the sounds placed in the world (SoundPlay::at, LiveSound::SetPosition). Several listeners are for split
    // screen: the one who hears a sound best decides. Moving them moves every live sound at once. Before the first
    // call there is one SoundListener at the origin. Throws std::invalid_argument for an invalid one.
    void SetListener(const SoundListener& listener);
    void SetListeners(std::span<const SoundListener> listeners);
    [[nodiscard]] std::span<const SoundListener> Listeners() const;

    // A bus's own level and muffling, for what a game does to its mix on purpose: lower the world under a pause menu,
    // muffle it when a character is stunned. It lies under the player's volume options and does not change them.
    void FadeBus(SoundBus bus, float volume, float seconds = 0.0f);
    void MuffleBus(SoundBus bus, float amount, float seconds = 0.0f);
    // Holds the live sounds of a bus in place and lets them go on later. Sounds from Play finish on their own. Game does
    // this for SoundBus::World whenever the game's time is paused.
    void SetBusPaused(SoundBus bus, bool paused);
    [[nodiscard]] bool BusPaused(SoundBus bus) const;
    // For development hosts: holds every live sound, on top of what SetBusPaused says.
    void SetHold(bool held);

    // Moves the cooldown clock and forgets sounds that have ended. Game calls this every frame with unscaled real time.
    void Update(float realSeconds);

private:
    struct Row {
        SoundId firstId{};
        std::size_t files = 1;
        SoundPitch pitch{};
        float cooldown = 0.0f;
        bool skipWhilePlaying = false;
        bool loop = false;
        SoundBus bus = SoundBus::World;
        SoundFalloff falloff{};
        double lastPlayed = -std::numeric_limits<double>::infinity();
        std::optional<std::size_t> lastFile{};
    };
    struct Table {
        const std::type_info* type = nullptr;
        std::vector<Row> rows;
    };
    [[nodiscard]] Row& RowOf(SoundKey key);
    [[nodiscard]] std::size_t PickFile(const Row& row);
    [[nodiscard]] bool RowPlaying(const Row& row) const;
    [[nodiscard]] VoiceStart Begin(Row& row, const SoundPlay& play);
    [[nodiscard]] LiveSound StartRow(Row& row, const SoundPlay& play, std::uint64_t startFrame);

    SoundOutput& _output;
    Rng _rng;
    std::vector<Table> _tables;
    double _seconds = 0.0;
    std::shared_ptr<LiveVoices> _live; // Shared so that a handle can outlive the player

public:
    SoundPlayer(const SoundPlayer&) = delete;
    SoundPlayer& operator=(const SoundPlayer&) = delete;
};
