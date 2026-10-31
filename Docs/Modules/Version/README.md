# Version

Source/API include root: `Engine/Core/Modules/Version/`. Documentation and plans live here;
`Internals/` and `Tests/` paths below refer to the source module directory.

Opt-in, headless tracking of the installed game version and queries over game-authored
patch notes. It does not contact an update server. Games that do not use it have no
runtime hooks, configuration fields, or tracking work.

## Public API

Include `Modules/Version/Version.h` and link `EngineVersion`. All operations are
static members of `Version`; there is no instance or automatic lifecycle registration.
`GameVersion`, `PatchNote`, and `Version::Change` are public value contracts.

```cpp
Version::Check(Options(), CurrentVersion, [this](const Version::Change& change) {
    if (change.kind == Version::ChangeKind::Upgrade) {
        const auto notes = Version::NotesSince(PatchNotesTable, change.previous);
        if (!notes.empty()) _mainMenuScene.OpenPatchNotes(notes);
    }
});
```

Call `Check` during game startup after options are loaded and response targets are
ready. It borrows the options and optional callback only for this synchronous call;
neither is retained. The game supplies its current version and response. Tracking,
comparison, encoding, and persistence belong to this module. A patch-note table is
not required. Construction of `Game` never invokes this module.

- First launch or invalid stored history: saves the current version without notifying.
- Unchanged version: no write or callback, including when other options are dirty.
- Changed version: saves the current version, then invokes the callback once with
  `Change { kind, previous, current }`. Kind is `Upgrade` or `Downgrade`.
- Empty callback: tracking still works, without a game response.
- Invalid current version: throws `std::invalid_argument` before changing options.
- Save failure: propagates the error, skips the callback, and restores the previous
  in-memory marker so the same options instance can retry. Other pending options
  remain in memory. A successful save also commits other dirty options in the file.
- Callback failure: propagates the error; the saved marker remains current, so the
  next launch will not repeat the callback.

The marker means the game started with this version, not that the player read or
dismissed patch notes. Call on the thread that owns the options; concurrent access
to the same options instance is unsupported.

`IsValid(version)` is constexpr: nonnegative `int` components, with minor and patch
at most 999. This is an integer storage constraint, not full SemVer support.
`Text(version)` formats `major.minor.patch`.

`PatchNotesValid(notes)` is constexpr and accepts only encodable versions in strictly
descending order, without duplicates; an empty table is valid. Use it in a
`static_assert` on authored tables. `NotesSince(notes, previous)` requires a valid
table and returns its prefix newer than `previous`, newest first. The span borrows
the input table; each note's strings also borrow the caller's storage. The game owns
titles, line formatting, localization, UI, and the choice of current version.

## Dependencies and storage

`EngineVersion` depends on headless `EngineHeadless` for `GameOptions`, which uses
Persistence. No raylib dependency. Core does not depend on this module. The target
is excluded from the default build unless selected by a consumer or its tests.

The module owns `app.last_version` in the supplied options file. Do not overwrite
this key from game code. Encoding remains `major * 1'000'000 + minor * 1'000 + patch`,
so existing installations need no migration. No extra file is created.

See `Tests/` for tracking, failure, and patch-note behavior tests.
