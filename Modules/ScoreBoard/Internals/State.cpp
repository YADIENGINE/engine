#include "Modules/ScoreBoard/Internals/State.h"
#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace {
using Document = JsonIO::Document;
constexpr std::int64_t SchemaVersion = 1;
constexpr std::string_view BoardKey = "scoreboard";

void RequireSchema(bool valid) {
    if (!valid) throw SaveError(SaveErrorCode::InvalidData, "Invalid ScoreBoard snapshot");
}
std::vector<ScoreBoard::Entry> DecodeBoard(const Document& board, const std::string& boardId) {
    RequireSchema(board.Members().size() == 3);
    RequireSchema(board.At("version").AsInt64() == SchemaVersion);
    RequireSchema(board.At("board_id").AsString() == boardId);
    const auto& rows = board.At("entries").Elements();
    std::vector<ScoreBoard::Entry> entries;
    entries.reserve(rows.size());
    std::uint64_t previous = 0;
    for (const auto& row : rows) {
        RequireSchema(row.Members().size() == 6);
        ScoreBoard::Entry entry{.result = {.score = row.At("score").AsInt64(),
                                           .playerId = row.At("player_id").AsString(),
                                           .playerName = row.At("player_name").AsString(),
                                           .metadata = row.At("metadata")},
                                .sequence = row.At("sequence").AsUInt64(),
                                .recordedAtUnixMs = row.At("recorded_at_unix_ms").AsInt64()};
        RequireSchema(!entry.result.playerId.empty() && entry.sequence > previous);
        previous = entry.sequence;
        entries.push_back(std::move(entry));
    }
    return entries;
}
bool AcceptsSnapshot(const Document::Object& values, const std::string& boardId) {
    const auto found = values.find(BoardKey);
    if (values.size() != 1 || found == values.end()) return false;
    try {
        (void)DecodeBoard(found->second, boardId);
        return true;
    } catch (const JsonIO::Error&) {
        return false;
    } catch (const SaveError&) {
        return false;
    }
}
SaveSettings StorageSettings(const ScoreBoard::Options& options) {
    if (options.boardId.empty()) throw std::invalid_argument("ScoreBoard requires a nonempty board ID");
    if (options.order != ScoreBoard::SortOrder::HighestFirst && options.order != ScoreBoard::SortOrder::LowestFirst)
        throw std::invalid_argument("Invalid ScoreBoard sort order");
    auto settings = options.persistence;
    auto callerSchema = std::move(settings.acceptsValues);
    settings.acceptsValues = [boardId = options.boardId,
                              callerSchema = std::move(callerSchema)](const Document::Object& values) {
        return AcceptsSnapshot(values, boardId) && (!callerSchema || callerSchema(values));
    };
    return settings;
}
Document EncodeBoard(const std::string& boardId, const std::vector<ScoreBoard::Entry>& entries) {
    Document::Array rows;
    rows.reserve(entries.size());
    for (const auto& entry : entries) {
        rows.emplace_back(Document::Object{{"score", entry.result.score},
                                           {"player_id", entry.result.playerId},
                                           {"player_name", entry.result.playerName},
                                           {"metadata", entry.result.metadata},
                                           {"sequence", entry.sequence},
                                           {"recorded_at_unix_ms", entry.recordedAtUnixMs}});
    }
    return Document::Object{{"version", SchemaVersion}, {"board_id", boardId}, {"entries", std::move(rows)}};
}
} // namespace

ScoreBoard::State::State(const Options& options)
    : _boardId(options.boardId)
    , _order(options.order)
    , _store(options.file, StorageSettings(options)) {
    if (_store.Load() != SaveLoadResult::Missing) _entries = DecodeBoard(*_store.Get(BoardKey), _boardId);
}
void ScoreBoard::State::Append(const ValidatedScore& result) {
    if (!_entries.empty() && _entries.back().sequence == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("ScoreBoard sequence exhausted");
    auto candidate = _entries;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    candidate.push_back({.result = result,
                         .sequence = _entries.empty() ? 1 : _entries.back().sequence + 1,
                         .recordedAtUnixMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count()});
    Commit(std::move(candidate));
}
std::vector<ScoreBoard::Entry> ScoreBoard::State::Ranked() const {
    auto ranked = _entries;
    std::sort(ranked.begin(), ranked.end(), [order = _order](const Entry& left, const Entry& right) {
        if (left.result.score == right.result.score) return left.sequence < right.sequence;
        return order == SortOrder::HighestFirst ? left.result.score > right.result.score
                                                : left.result.score < right.result.score;
    });
    return ranked;
}
std::vector<ScoreBoard::Entry> ScoreBoard::State::PerPlayer() const {
    std::unordered_set<std::string> players;
    std::vector<Entry> result;
    for (auto& entry : Ranked()) {
        if (players.insert(entry.result.playerId).second) result.push_back(std::move(entry));
    }
    return result;
}
void ScoreBoard::State::Reset() { Commit({}); }
void ScoreBoard::State::Commit(std::vector<Entry> candidate) {
    // Persistence may have committed the primary before a backup-refresh failure.
    // Queries keep the previous snapshot unless the entire Save returns successfully.
    _store.Set(std::string(BoardKey), EncodeBoard(_boardId, candidate));
    _store.Save();
    _entries.swap(candidate);
}
