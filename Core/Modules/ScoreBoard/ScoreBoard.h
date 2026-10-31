#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Modules/Persistence/Persistence.h"

// One locally persisted board. Derive only to supply the game's validation policy.
class ScoreBoard {
public:
    enum class SortOrder { HighestFirst, LowestFirst };

    struct Input {
        std::int64_t score = 0;
        std::string playerId;
        std::string playerName;
        JsonIO::Document evidence;
    };

    struct ValidatedScore {
        std::int64_t score = 0;
        std::string playerId;
        std::string playerName;
        JsonIO::Document metadata;
    };

    struct Entry {
        ValidatedScore result;
        // Positive, board-local insertion order; starts again at one after reset.
        std::uint64_t sequence = 0;
        std::int64_t recordedAtUnixMs = 0;
    };

    struct Options {
        std::string boardId;
        std::filesystem::path file;
        SortOrder order = SortOrder::HighestFirst;
        SaveSettings persistence{.keepBackup = true};
    };

    virtual ~ScoreBoard();
    // False means validation rejection. Technical failures throw, including SaveError.
    [[nodiscard]] bool ScoreToBoard(const Input& input);
    // Owned, ranked snapshots. Tied scores retain their original insertion order.
    [[nodiscard]] std::vector<Entry> GetScoreBoard() const;
    [[nodiscard]] std::vector<Entry> GetScoreBoardN(std::size_t count) const;
    [[nodiscard]] std::vector<Entry> GetScoreBoardPerPlayer() const;
    // Immediately saves an empty board; does not remove files or reset encryption keys.
    void ResetScoreBoard();

protected:
    // Loads existing data without calling ValidateScore; a missing file stays absent.
    explicit ScoreBoard(const Options& options);
    [[nodiscard]] virtual std::optional<ValidatedScore> ValidateScore(const Input& input) const = 0;

private:
    class State;
    std::unique_ptr<State> _state;

public:
    ScoreBoard(const ScoreBoard&) = delete;
    ScoreBoard& operator=(const ScoreBoard&) = delete;
    ScoreBoard(ScoreBoard&&) = delete;
    ScoreBoard& operator=(ScoreBoard&&) = delete;
};
