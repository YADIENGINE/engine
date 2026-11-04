#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <typeinfo>
#include <vector>
#include "Audio/SoundKey.h"
#include "Audio/SoundOutput.h"
#include "Audio/SoundTables.h"
#include "Utilities/Rng.h"

// Plays the rows of the game's sound tables: `game.Sounds().Play(Sfx::Hit)`. It picks the file and the pitch and keeps
// the cooldowns; only the last step reaches the device. Use it from the thread that owns the audio. Not for a
// simulation: a sound is something a view does about what the simulation said.
class SoundPlayer {
public:
    static constexpr std::uint64_t DefaultSeed = 0x536F756E64; // Fixed: nobody can hear that two runs pick alike

    SoundPlayer(const SoundTables& tables, SoundOutput& output, std::uint64_t seed = DefaultSeed);

    // Whether the key names a row of a registered table. It asks the tables, not the device, so a system can check its
    // keys at startup, before audio has loaded. An empty key: false.
    [[nodiscard]] bool Has(SoundKey key) const;
    // Whether any file of the row is playing now; false for a key Has rejects. For "never overlap itself" whatever the
    // row says: `if (!sounds.IsPlaying(key)) sounds.Play(key);`.
    [[nodiscard]] bool IsPlaying(SoundKey key) const;
    // Plays the row at its picked pitch times `pitch`. False when the row's cooldown or SoundOverlap::Skip stopped it.
    // Throws std::out_of_range for a key Has rejects (a table that was never registered, an empty key, Enum::Count),
    // std::invalid_argument for a pitch that is not finite and positive, and std::logic_error before audio has loaded.
    bool Play(SoundKey key, float pitch = 1.0f);
    // Moves the cooldown clock. Game calls this every frame with unscaled real time.
    void Update(float realSeconds);

private:
    struct Row {
        SoundId firstId{};
        std::size_t files = 1;
        SoundPitch pitch{};
        float cooldown = 0.0f;
        bool skipWhilePlaying = false;
        double lastPlayed = -std::numeric_limits<double>::infinity();
        std::optional<std::size_t> lastFile{};
    };
    struct Table {
        const std::type_info* type = nullptr;
        std::vector<Row> rows;
    };
    [[nodiscard]] std::size_t PickFile(const Row& row);
    [[nodiscard]] bool RowPlaying(const Row& row) const;

    SoundOutput& _output;
    Rng _rng;
    std::vector<Table> _tables;
    double _seconds = 0.0;

public:
    SoundPlayer(const SoundPlayer&) = delete;
    SoundPlayer& operator=(const SoundPlayer&) = delete;
};
