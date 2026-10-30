# RunTracking

Opt-in, game-independent run archive. The game supplies an owned `JsonIO::Document`;
RunTracking writes compact JSON in one durable, atomically replaced gzip file per run.
It does not define gameplay rules, record fields, schema versions or run-end detection.

## Targets and dependencies

- `EngineRunTracking`: headless archive, IDs and UTC timestamps. Its sole module dependency
  is public `EngineJsonIO`, used for documents, serialization, gzip and atomic file replacement.
- `EngineRunTrackingRuntime`: optional application-context bridge. Links `EngineRunTracking`
  and the runtime `Engine`; only this target compiles the private `EngineGodAccessor`.

Both targets are excluded from the default build unless linked. Neither `EngineHeadless` nor
`Engine` links or starts tracking. Consumers include only `Modules/RunTracking/RunTracking.hpp`.

```cmake
target_link_libraries(MyCore PUBLIC EngineRunTracking)
# Only applications using Context(game) or Directory(game):
target_link_libraries(MyApplication PRIVATE EngineRunTrackingRuntime)
```

## Public API

```cpp
#include "Modules/RunTracking/RunTracking.hpp"

RunTracking archive(directory);
const auto runId = RunTracking::NewRunId(); // Once at run start; keep it for retries.
auto record = JsonIO::Document::MakeObject();
record["run_id"] = runId;
record["ended_at_utc"] = RunTracking::UtcNow();
record["score"] = 120; // Every record field belongs to the game.
const auto file = archive.Write(runId, record);
```

| Operation | Contract |
| --- | --- |
| `RunTracking(directory)` | Stores the absolute archive path. Empty paths throw `std::invalid_argument`; no directory or file is created yet. |
| `Write(runId, record)` | Creates the directory and writes `<runId>.json.gz`. Returns its absolute path. The same ID atomically replaces the same file. |
| `Directory() const` | Borrows the stored absolute path, valid for the archive object's lifetime. |
| `NewRunId()` | Produces a file-safe ID independently of gameplay RNGs, using time, a process salt and an atomic sequence. |
| `UtcNow()` | Returns current UTC as `YYYY-MM-DDTHH:MM:SSZ`. |
| `Context(game)` | Runtime bridge: returns an owned document containing `application_id` and `platform`. |
| `Directory(game)` | Runtime bridge: returns `game`'s user-data path for `runs`. |

Run IDs must contain 1..120 ASCII letters, digits, hyphens or underscores; other values throw
`std::invalid_argument`. A run ID is not inferred from or compared with the record's fields.

The archive owns its path and retains no references to the record or Game. Those inputs must
remain valid for the call. There is no initialization/shutdown registration, background worker,
retry queue, retention policy or destructor IO. The game owns those policies and decides when to write.
Observe a live Game on its runtime thread; callers coordinate concurrent writes to the same run ID.

## Failures and format

Writing is synchronous. Filesystem failures and `JsonIO::Error` propagate to the caller;
an invalid document does not replace an existing good archive. The game may retain the same
ID and document to retry at its own error boundary. `NewRunId` may also propagate random-source
or allocation failures. No operation converts failures into gameplay outcomes.

Serialization and compression finish in memory before JsonIO replaces the destination using a
unique temporary file in the same directory. Writes use compact JSON and gzip, without a trailing
newline. Standard gzip readers can recover the JSON; `JsonIO::Read` does not decompress gzip.
The write limit for serialized bytes is raised to `size_t`'s maximum; the normal nesting limit
and gzip compressor limits still apply. Persistence's backup/recovery policy is not used.

Tests live in `Modules/RunTracking/Tests`, link only the headless target and exercise public API.
See [JsonIO](../JsonIO/README.md) and its [third-party notices](../JsonIO/ThirdParty.md).
