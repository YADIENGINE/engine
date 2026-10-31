# Persistence

Source/API include root: `Engine/Core/Modules/Persistence/`. Documentation and plans live here;
`Internals/` and `Tests/` paths below refer to the source module directory.

One owned, in-memory JSON value store per file, with explicit saving, platform-protected
key management, verified backups and automatic recovery. Include only
`Modules/Persistence/Persistence.h`; link `EnginePersistence`.

## Public API

`Persistence(path, SaveSettings)` copies its settings and resolves the explicit path.
Construction/destruction do not read, save or create keys/files. Instances cannot be
copied; moving transfers ownership. A moved-from instance supports destruction or
move assignment only. There is no autosave or mutable global service.

| Operation | Behavior |
| --- | --- |
| `Load()` | Validates and loads the primary; automatically tries a verified backup when keepBackup is enabled. |
| `Save()` | Persists current values through JsonIO, maintaining backups when enabled. |
| `Get(key)` | Returns an owned optional Document copy; missing differs from a present JSON null. |
| `Set(key, document)` | Replaces one in-memory value; does not save. |
| `Contains(key)` / `Erase(key)` | Query/remove a literal top-level key; Erase does not save. |
| `GetInt(key, fallback)` / `SetInt(key, value)` | Integer convenience operations used by engine/game consumers. Fallback applies only to absence; incompatible/out-of-range values are errors. |
| `Path()` / `BackupPath()` | Primary path and sibling filename with `.bak` appended. Path's borrowed reference lives until instance destruction/move assignment. |
| `ArchiveFiles()` | Archives existing primary/backup under unique `.invalid` names; preserves loaded values. |
| `RemoveFiles()` | Explicitly removes primary/backup; preserves loaded values. |
| `ResetEncryptionKey()` | Archives the shared key, resets its platform wrapping alias; never called automatically. |

Keys are nonempty UTF-8 JSON strings. Dots/slashes have no path semantics. Values
can be any public `JsonIO::Document`, including nested objects, arrays and null.
Get returns copies so editing a retrieved document cannot bypass Set. Document
semantics are described in [JsonIO](../JsonIO/README.md).

```cpp
SaveSettings settings{
    .encryption = SaveEncryptionMode::Encrypted,
    .keepBackup = true,
    .keys = {.file = userData / "keys/save.key"}
};
Persistence store(userData / "progress.json", settings);
const auto result = store.Load();
store.Set("profile", JsonIO::Document::Object{{"name", "Example"}, {"enabled", true}});
store.SetInt("eggs", store.GetInt("eggs") + 1);
store.Save();
```

### Settings and file compatibility

SaveSettings defaults to plaintext, a 1 MiB stored-file limit, no backup, and 128
nested JSON containers (the persistence envelope consumes two). maxFileBytes must
be positive; maxDepth must be at least two. All settings are owned copies.
Encrypted stores require keys.file. Multiple instances may specify the same
key file without sharing an in-memory service.

The JSON envelope remains `{"version":2,"values":{...}}`; existing integer records
and encrypted envelopes remain readable. Values now support every JSON type.
Version 1 reports Outdated, which never triggers backup recovery. Other unsupported
versions and malformed envelope structures report InvalidData. Value meanings and
application-specific schema checks remain the caller's responsibility.

The envelope version describes the storage format, not the game release or the
number of saved fields. Adding a stat or setting does not require changing it.
Missing keys use `GetInt`'s caller-supplied fallback (zero by default); additional
keys are accepted and preserved. Changing an existing key's type or meaning is a
separate compatibility change, not an additive field.

Optional `acceptsValues` supplies a caller-owned schema predicate over a const value
map. It must be side-effect-free and return false to reject a snapshot; returning
false reports InvalidData. It runs before save/load/recovery commits and applies
equally to backups. The copied callable and its captures live with the instance;
callers must keep any borrowed captures alive. Unexpected predicate exceptions
propagate. A consumer can require strings for equipment keys and integers for counters,
or store arbitrary JSON values without a predicate. The game owns those key/schema choices.

Encrypted stores reject plaintext records with EncryptionRequired. Plaintext stores
reject encrypted records with InvalidData. There is no format conversion,
plaintext-to-encrypted import, legacy reset allowlist, or cross-file key migration.
Authentication failure never tries plaintext. File replacement and player-facing
recovery policy belong to the consuming application.

### Load results, recovery and errors

- Loaded: valid primary loaded.
- Missing: the primary and any enabled backup do not exist; values become empty. No file/key is created.
- Recovered: a valid backup restored the primary; an existing damaged primary was archived.

Every unsuccessful load preserves current values. Missing is a successful empty
load. A corrupt/unavailable backup is never treated as an empty new installation.
Recovery validates the same schema, size/depth, encryption mode and authentication as
normal loading. IO, platform and outdated-schema failures
stop the operation. Technical errors never decide whether a player starts over.

SaveError carries SaveErrorCode through Code(): InvalidData, AuthenticationFailed,
EncryptionRequired, KeyUnavailable, Io, PlatformUnavailable, Outdated.
Invalid arguments such as empty keys/paths or encrypted stores without key settings
throw standard argument errors. Allocation failures may propagate.

Saving first validates the new document and, when possible, seeds a verified backup
from the old record before primary replacement. It then commits the new primary
atomically and refreshes the backup. These are separate commits: a backup-refresh
failure may follow a successful primary write. Encrypted backups are independently
authenticated snapshots; their nonces/ciphertext need not equal the primary's.
Failed restoration can leave the original at its archive path, with the verified
backup intact. ArchiveFiles/RemoveFiles are separate per-file operations too.

## Keys, dependencies and lifecycle

The module owns key lookup/creation. Reads never generate a missing key. Encrypted
writes create one only when absent; broken keys are never silently replaced.
Temporary raw-key buffers are wiped after use. Plaintext paths never access keys.

- Windows: the existing noninteractive user-scoped DPAPI protection is retained.
- Linux: owned, regular, nonsymlink raw-key file, 0600, with a 0700 key directory.

ResetEncryptionKey invalidates **all** older encrypted records sharing that key,
including other Persistence instances. The application must request this explicitly.
Archiving/removing records does not reset keys or loaded values.

Dependencies are JsonIO's public API for all JSON/crypto operations, plus shared
Platform file/random primitives for protected key files and archival operations.
Internals never include another module's Internals. Persistence does not know Game,
player state, settings names, version notifications or run-statistics schemas.

Instances are not thread-safe; synchronize shared-instance operations externally.
Same-path writers and shared-key reset/write operations require caller coordination.
Atomic file replacement does not provide rollback prevention or multi-file transactions.
