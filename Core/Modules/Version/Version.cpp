#include "Modules/Version/Version.h"
#include <algorithm>
#include <array>
#include <charconv>
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

GameVersion Version::Parse(std::string_view text) {
    std::array<int, 3> parts{};
    std::size_t count = 0;
    std::size_t start = 0;
    while (true) {
        const auto end = text.find('.', start);
        const auto part = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
        int number = 0;
        const auto [last, error] = std::from_chars(part.data(), part.data() + part.size(), number);
        if (part.empty() || error != std::errc{} || last != part.data() + part.size() || number < 0 || count == 4)
            throw std::invalid_argument("Version::Parse: \"" + std::string(text) + "\" is not a version such as 1.2.3");
        if (count < parts.size()) parts[count] = number;
        ++count;
        if (end == std::string_view::npos) break;
        start = end + 1;
    }
    const GameVersion version{parts[0], parts[1], parts[2]};
    if (!IsValid(version))
        throw std::invalid_argument("Version::Parse: \"" + std::string(text) + "\" is outside the version limits");
    return version;
}

std::span<const PatchNote> Version::NotesSince(std::span<const PatchNote> notes, GameVersion previous) {
    const auto firstSeen =
        std::ranges::find_if(notes, [previous](const PatchNote& note) { return note.version <= previous; });
    return {notes.begin(), firstSeen};
}
