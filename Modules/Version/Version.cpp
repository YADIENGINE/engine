#include "Modules/Version/Version.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include "Options/GameOptions.h"

namespace {
constexpr std::string_view LastVersionKey = "app.last_version";
constexpr std::int64_t MajorStep = 1'000'000;
constexpr std::int64_t MinorStep = 1'000;
constexpr std::int64_t Pack(GameVersion version) {
    return version.major * MajorStep + version.minor * MinorStep + version.patch;
}
constexpr GameVersion Unpack(std::int64_t packed) {
    return {static_cast<int>(packed / MajorStep), static_cast<int>(packed / MinorStep % MinorStep),
            static_cast<int>(packed % MinorStep)};
}
} // namespace

void Version::Check(GameOptions& options, GameVersion current, const std::function<void(const Change&)>& onChange) {
    if (!IsValid(current)) throw std::invalid_argument("Invalid installed version");
    const bool hasPrevious = options.Contains(LastVersionKey);
    const auto previous = options.GetInt(LastVersionKey, -1);
    const auto packed = Pack(current);
    if (hasPrevious && previous == packed) return;
    const bool validPrevious = hasPrevious && previous >= 0 && previous / MajorStep <= std::numeric_limits<int>::max();
    options.SetInt(std::string(LastVersionKey), packed);
    try {
        options.Save();
    } catch (...) {
        // Keep a failed write retryable on this same options instance.
        if (hasPrevious)
            options.SetInt(std::string(LastVersionKey), previous);
        else
            options.Erase(LastVersionKey);
        throw;
    }
    if (validPrevious && onChange) {
        const auto before = Unpack(previous);
        onChange({current > before ? ChangeKind::Upgrade : ChangeKind::Downgrade, before, current});
    }
}

std::string Version::Text(GameVersion version) {
    return std::to_string(version.major) + "." + std::to_string(version.minor) + "." + std::to_string(version.patch);
}

std::span<const PatchNote> Version::NotesSince(std::span<const PatchNote> notes, GameVersion previous) {
    const auto firstSeen =
        std::ranges::find_if(notes, [previous](const PatchNote& note) { return note.version <= previous; });
    return {notes.begin(), firstSeen};
}
