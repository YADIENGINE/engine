#pragma once
#include <compare>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>

class GameOptions;

struct GameVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
    constexpr auto operator<=>(const GameVersion&) const = default;
};

struct PatchNote {
    GameVersion version;
    std::string_view title;
    std::string_view lines;
};

// Installed-version tracking; no automatic runtime registration or remote update checks.
class Version final {
public:
    enum class ChangeKind { Upgrade, Downgrade };

    struct Change {
        ChangeKind kind;
        GameVersion previous;
        GameVersion current;
    };

    // Saves before notifying. First launch/invalid history records silently; unchanged versions do no IO.
    // The callback is synchronous and optional. Save/callback errors propagate to the caller.
    static void Check(GameOptions& options, GameVersion current,
                      const std::function<void(const Change&)>& onChange = {});
    [[nodiscard]] static std::string Text(GameVersion version);
    // Notes must be valid and newest first. Returned views borrow the caller's table and strings.
    [[nodiscard]] static std::span<const PatchNote> NotesSince(std::span<const PatchNote> notes, GameVersion previous);
    [[nodiscard]] static constexpr bool IsValid(GameVersion version) {
        return version.major >= 0 && version.minor >= 0 && version.minor <= 999 && version.patch >= 0 &&
               version.patch <= 999;
    }
    [[nodiscard]] static constexpr bool PatchNotesValid(std::span<const PatchNote> notes) {
        for (std::size_t i = 0; i < notes.size(); ++i) {
            if (!IsValid(notes[i].version)) return false;
            if (i > 0 && !(notes[i].version < notes[i - 1].version)) return false;
        }
        return true;
    }
};
