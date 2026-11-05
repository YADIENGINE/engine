#pragma once
#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string_view>
#include "Audio/SoundBus.h"
#include "Audio/SoundKey.h"
#include "Audio/SoundSpace.h"

// A row's files, relative to the asset root. The fixed size keeps a row constexpr: a ninth file fails to compile.
inline constexpr std::size_t MaxSoundVariants = 8;
using SoundFiles = std::array<std::string_view, MaxSoundVariants>;

// What a new play does while the row is already playing. One value, so "skip, with four copies" cannot be written.
class SoundOverlap {
public:
    // Up to this many copies of each file at once; one more restarts the next copy of that file.
    [[nodiscard]] static constexpr SoundOverlap Copies(std::size_t copies) {
        if (copies == 0) throw std::invalid_argument("SoundOverlap::Copies must be at least 1");
        return SoundOverlap(copies);
    }
    // While any file of the row is playing, the new play is dropped and Play returns false.
    static const SoundOverlap Skip;

    [[nodiscard]] constexpr bool IsSkip() const { return _copies == 0; }
    // Voices each file needs.
    [[nodiscard]] constexpr std::size_t Voices() const { return IsSkip() ? 1 : _copies; }

private:
    explicit constexpr SoundOverlap(std::size_t copies)
        : _copies(copies) {}
    std::size_t _copies;
};
inline constexpr SoundOverlap SoundOverlap::Skip = SoundOverlap(0);

// The range a play's pitch is picked from, uniformly. Changing pitch also changes speed.
struct SoundPitch {
    float low = 1.0f;
    float high = 1.0f;
};

struct SoundOptions {
    float volume = 1.0f;
    SoundPitch pitch{};
    // Real seconds after a play during which Play returns false; hitstop and slow motion do not stretch it.
    float cooldown = 0.0f;
    SoundOverlap whenPlaying = SoundOverlap::Copies(4);
    bool loop = false;     // Plays forever: SoundPlayer::Start only, so that something can stop it
    bool streamed = false; // Read from disk while it plays, not loaded into memory: for long files
    SoundBus bus = SoundBus::World;
    SoundFalloff falloff{}; // How it fades with distance when played at a position in the world
    std::string_view label; // For tools; a row without one gets a generated label
};

// Relative, inside the asset root: the rules of AudioSystem::ValidateCatalog, which also runs at startup.
[[nodiscard]] constexpr bool SoundFileInsideAssets(std::string_view file) {
    if (file.empty() || file[0] == '/' || file[0] == '\\' || (file.size() > 1 && file[1] == ':')) return false;
    while (!file.empty()) {
        const std::size_t end = file.find_first_of("/\\");
        if (file.substr(0, end) == "..") return false;
        file = end == std::string_view::npos ? std::string_view{} : file.substr(end + 1);
    }
    return true;
}

// Throws std::invalid_argument. In a constexpr table the throw is a compile error that points at the row.
constexpr void CheckSoundRow(const SoundFiles& files, const SoundOptions& options) {
    constexpr float Largest = std::numeric_limits<float>::max();
    if (files[0].empty()) throw std::invalid_argument("A sound row needs a file");
    for (std::size_t i = 1; i < files.size(); ++i) {
        if (files[i].empty()) continue;
        if (files[i - 1].empty()) throw std::invalid_argument("A sound row's files must have no gap");
        for (std::size_t j = 0; j < i; ++j)
            if (files[j] == files[i]) throw std::invalid_argument("A sound row lists the same file twice");
    }
    for (const std::string_view file : files)
        if (!file.empty() && !SoundFileInsideAssets(file))
            throw std::invalid_argument("A sound file must be a relative path inside the asset root");
    if (!(options.volume >= 0.0f && options.volume <= 1.0f))
        throw std::invalid_argument("A sound row's volume must be between 0 and 1");
    if (!(options.pitch.low > 0.0f && options.pitch.high >= options.pitch.low && options.pitch.high <= Largest))
        throw std::invalid_argument("A sound row's pitch range must be positive, with low <= high");
    if (!(options.cooldown >= 0.0f && options.cooldown <= Largest))
        throw std::invalid_argument("A sound row's cooldown must be finite and not negative");
    if (!IsValid(options.falloff))
        throw std::invalid_argument("A sound row's falloff needs 0 <= fullWithin < silentBeyond");
    if (options.bus != SoundBus::World && options.bus != SoundBus::Ui && options.bus != SoundBus::Music)
        throw std::invalid_argument("A sound row names an unknown bus");
}

// One sound of a game's table. `id` is its enum value, so that static_assert(RowsInIdOrder(table)) works. Invalid data
// fails to compile in a constexpr table and throws std::invalid_argument anywhere else.
template <SoundEnum Enum> struct SoundRow {
    constexpr SoundRow(Enum rowId, const SoundFiles& rowFiles, const SoundOptions& rowOptions = {})
        : id(rowId)
        , files(rowFiles)
        , options(rowOptions) {
        CheckSoundRow(files, options);
    }
    Enum id;
    SoundFiles files;
    SoundOptions options;
};
