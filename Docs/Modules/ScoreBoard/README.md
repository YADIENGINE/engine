# ScoreBoard

Source/API include root: `Engine/Core/Modules/ScoreBoard/`. Documentation and plans live here;
`Internals/` and `Tests/` paths below refer to the source module directory.

One game-independent, locally persisted score history per board. The module owns
validation dispatch, recording, ranking and storage. Include only
`Modules/ScoreBoard/ScoreBoard.hpp`; link `EngineScoreBoard`. No runtime, raylib or
Game dependency is required.

## Public API

Derive from `ScoreBoard` and implement the protected, const
`ValidateScore(const Input&) -> std::optional<ValidatedScore>` policy. Construct the
base with `Options`; all service operations remain nonvirtual.

| Operation | Behavior |
| --- | --- |
| `ScoreToBoard(input)` | Calls the game's validator exactly once. Returns false on nullopt or an empty approved playerId, with no write. Saves immediately and returns true on success. Technical failures throw. |
| `GetScoreBoard()` | Returns every entry in score order. |
| `GetScoreBoardN(count)` | Returns up to count entries in the same order; zero returns empty. |
| `GetScoreBoardPerPlayer()` | Returns each player's best entry, in score order. Groups by playerId, never display name. |
| `ResetScoreBoard()` | Immediately saves an empty board, including its backup when enabled. Resets sequence numbering to one for the next accepted entry. |

### Value contracts

All strings, JSON values, settings and returned lists are owned copies.

- `Input`: signed 64-bit `score`, stable `playerId`, display `playerName` and
  `JsonIO::Document evidence`. Games prepare this data, including any game-specific
  validation evidence. The input is borrowed only for the submission call.
- `ValidatedScore`: approved `score`, `playerId`, `playerName` and JSON `metadata`.
  The validator may normalize any field. Only this returned data is recorded;
  evidence is never copied into storage automatically. Empty approved playerId is
  rejected. Display names may be empty; zero and negative scores are valid.
- `Entry`: `result` (ValidatedScore), positive board-local `uint64_t sequence` and
  `int64_t recordedAtUnixMs` (UTC Unix milliseconds from the system clock).
  Sequence is insertion order, not a globally unique identifier, and is reused
  after reset. Sequence exhaustion throws `std::overflow_error` without a write.
- `Options`: nonempty `boardId`, explicit `file`, `order` and `persistence` settings.
  `SortOrder::HighestFirst` is default; `LowestFirst` supports lower-is-better
  scores. Reopening the same board with another order changes only query order.

Every accepted call appends a distinct entry, even for identical inputs. All
history is retained; top-N and per-player queries never trim stored data. Tied
scores use ascending sequence, so the earliest accepted result wins, including
after reopening. Wall-clock changes do not affect ranking. Queries return owned
snapshots and do not perform IO or invoke the validator.

```cpp
#include "Modules/ScoreBoard/ScoreBoard.hpp"

class ExampleBoard final : public ScoreBoard {
public:
    explicit ExampleBoard(const Options& options);

protected:
    std::optional<ValidatedScore> ValidateScore(const Input& input) const override;
};

ExampleBoard::ExampleBoard(const Options& options)
    : ScoreBoard(options) {}

std::optional<ScoreBoard::ValidatedScore> ExampleBoard::ValidateScore(const Input& input) const {
    // The consuming game defines the evidence contract and its checks.
    if (!input.evidence.At("eligible").AsBool()) return std::nullopt;
    return ValidatedScore{.score = input.score,
                          .playerId = input.playerId,
                          .playerName = input.playerName,
                          .metadata = JsonIO::Document::Object{{"rules_version", 1}}};
}

void RecordExample(const std::filesystem::path& userData) {
    ExampleBoard board({.boardId = "solo", .file = userData / "solo-scoreboard.json"});
    const bool saved = board.ScoreToBoard({.score = 120,
                                          .playerId = "local-profile",
                                          .playerName = "Player",
                                          .evidence = JsonIO::Document::Object{{"eligible", true}}});
    const auto topTen = board.GetScoreBoardN(10);
    const auto personalBests = board.GetScoreBoardPerPlayer();
    const auto allResults = board.GetScoreBoard();
    // Consume saved/lists in the game. No separate Load or Save call is needed.
    (void)saved;
    (void)topTen;
    (void)personalBests;
    (void)allResults;
}
```

## Persistence, errors and lifecycle

Construction loads the board, including automatic schema-checked recovery when
backups are enabled. It never invokes virtual game validation. Missing files
produce an empty board without creating files or keys. Destruction performs no
IO. Instances are neither copyable nor movable. Stored results receive structural
validation on load, not replay of the game's current validator: evidence was not
retained.

`Options::persistence` defaults to plaintext, backup enabled and Persistence's
normal 1 MiB / 128-container limits. Encryption, keys, limits and other settings
are copied and delegated to Persistence. If acceptsValues is supplied, it is
combined with the module's schema check (module check first), never replaced or
bypassed. Its borrowed captures must outlive the board. No board schema migration,
silent reset or automatic history eviction is provided. Reaching a file limit
fails the write; callers can explicitly raise the limit.

Each board owns a dedicated Persistence file. Its `values` map contains only
`scoreboard`, whose value is `{version: 1, board_id: string, entries: array}`.
Rows contain `score`, `player_id`, `player_name`, `metadata`, `sequence` and
`recorded_at_unix_ms`. Sequences must be positive and strictly increasing in the
stored insertion order. Version, board identity, required fields/types and
sequence order are checked through Persistence's acceptsValues predicate before
normal load, recovery or saving commits. Malformed records and another board's
file are rejected, not treated as a new installation. JSON metadata may contain
any supported JsonIO value, subject to storage limits.

A candidate snapshot is fully prepared before saving. It becomes queryable only
after Persistence::Save returns successfully. On failure, queries retain the last
successful snapshot and a later write rebuilds from it; failed candidates are not
silently flushed later. Reset uses the same rule.

Storage failures propagate as `SaveError` with their original code. Invalid
options throw standard argument errors; unexpected validator/caller-predicate
exceptions and allocation failures propagate. None mean validation rejection.
See [Persistence](../Persistence/README.md) for its detailed failure contract:
the primary write can succeed before a backup refresh fails. A thrown operation
therefore does **not** guarantee unchanged disk state, and queries on the existing
board can differ from a newly opened board. Recreate the instance to reconcile
with disk before deciding whether to retry; repeated submissions are intentionally
not deduplicated. Reset never removes files, archives files or resets keys.

Instances are not thread-safe or reentrant. Coordinate calls externally. Use one
writer per file; multiple live instances do not synchronize their memory. Choose
a different boardId and file for each mode/difficulty whose results should be
separate. Player identity and the truth of game evidence are the game's policy;
this module does not supply anti-cheat or online authentication.

## Dependencies and tests

- [Persistence](../Persistence/README.md) public API: owned local storage, optional
  encryption, atomic writes, backups and schema-checked recovery.
- [JsonIO](../JsonIO/README.md) public value contract: game evidence, approved
  metadata and private serialization. Its backend objects are never exposed.
- C++20 standard library for owned values, ranking and timestamps. No other
  module's Internals are included. `EngineScoreBoard` publicly links
  `EnginePersistence`, which provides JsonIO transitively; it can be linked without
  EngineHeadless or Game.

`Tests/` registers `EngineScoreBoardTests` automatically when ENGINE_BUILD_TESTS is
enabled, tagged `[scoreboard]`. Tests use temporary files and the public APIs of
this module and its declared dependencies. They cover validation, queries,
durability, schema recovery and write failures without repeating crypto tests.
