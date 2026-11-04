# Audio

The engine owns the audio device and resources; the game decides what plays and when. The single public entry point is
[AudioSystem.h](../Core/Audio/AudioSystem.h): the service, IDs, catalog, and settings are in this header. The header
includes neither raylib nor the loading thread. `Audio/Internals/` is not supported API; games and editors use only the
public header. Audio is an engine runtime service, not a separate opt-in module.

A game's sound effects belong in [sound tables](#sound-tables); `AudioSystem` underneath plays them. Music, and any effect
a game prefers to number by hand, goes in a [catalog](#setting-up-a-catalog).

## Sound tables

```cpp
#include "Runtime/Game.h"

enum class Sfx { Hit, Coin, UiMove, Count };
inline constexpr std::array SfxTable{
    SoundRow{Sfx::Hit, {"Audio/hit1.wav", "Audio/hit2.wav"}, {.pitch = {0.95f, 1.05f}, .cooldown = 0.03f, .label = "Hit"}},
    SoundRow{Sfx::Coin, {"Audio/coin.wav"}},
    SoundRow{Sfx::UiMove, {"Audio/move.wav"}, {.whenPlaying = SoundOverlap::Skip}},
};
static_assert(RowsInIdOrder(SfxTable));

settings.sounds = SfxTable;     // In the game's settings
game.Sounds().Play(Sfx::Hit);   // In a scene holding a Game&; in an editor scene, services.sounds.Play(Sfx::Hit)
```

A row is `SoundRow{id, files, options}`; `SoundRow<Sfx>{...}` means the same.

| Option | Default | Meaning |
| --- | --- | --- |
| files | - | One to `MaxSoundVariants` (8) paths relative to the asset root. One is picked at random for each play, never the same one twice in a row |
| `volume` | 1 | 0 to 1, under the player's effect volume |
| `pitch` | `{1, 1}` | A range; each play picks from it. Pitch also changes speed |
| `cooldown` | 0 | Real seconds after a play during which the row refuses another; hitstop, slow motion and the editor's speed do not stretch it |
| `whenPlaying` | `SoundOverlap::Copies(4)` | `Copies(n)`: up to n copies of each file, and one more restarts a copy. `SoundOverlap::Skip`: while any file of the row plays, a new play is dropped |
| `label` | generated | The name tools show. Without one: `Sfx[0]`. A row with several files adds `(1/2)` to each |

`Play(key, pitch = 1)` multiplies the picked pitch by `pitch`, and returns false when a cooldown or the skip rule stopped
it. A play that was stopped uses up nothing: no cooldown starts and the last file stays the last file. The choices come
from the player's own `Rng` with a fixed seed. Playing before audio has loaded throws `std::logic_error`, as `PlayEffect` does.

**What fails, and when:**

| Mistake | Fails |
| --- | --- |
| A misspelled sound; an enum value without a row; a ninth file | Compile (at the assignment to `settings.sounds` for the row count) |
| Rows out of order | Compile with the `static_assert`; startup without it |
| Bad volume, pitch, cooldown or copies; no file, a gap, or one file twice in a row; an absolute or `..` path | Compile in a `constexpr` table; throws `std::invalid_argument` when the row is made otherwise |
| Two tables for one enum; a `settings.audio` effect ID in the reserved range | Startup, `std::invalid_argument` |
| A sound file that is missing or unreadable | Loading, as for any catalog entry |
| Playing an enum whose table was never registered | **That play**, `std::out_of_range` |

The last one is the remaining hole: a key does not know at compile time which tables a game registered. `Has` lets a
system check its keys at startup.

**Several tables:** `settings.sounds = {SfxTable, UiTable};`, one per enum type.

**A system that knows no game:** a `SoundKey` is made implicitly from any table enum, and is what such a system stores.
`SoundKey{}` is empty: `if (key) sounds.Play(key);`. `Has(key)` asks the tables, not the device, so it works before audio
has loaded. An enum without `Count`, such as `SoundId`, does not convert to a key.

**Under it:** each file of each row becomes a catalog entry with an ID from `SoundTables::FirstGeneratedId` (2^32) up, so
`audio catalog`, `audio usage` and the Audio Preview panel show them with their labels. A game's own low IDs in
`settings.audio` cannot meet them; an ID at or above that is rejected at startup. `CatalogWithSounds` gives the whole
catalog. `SoundPlayer` builds in `EngineHeadless` and reaches the device only through `SoundOutput`, so its decisions are
tested with no device; `Game` connects it to `AudioSystem`. A simulation cannot include any of `Audio/`
(`python Engine check`) or link it. To play a table's sound, go through the player: its IDs are not for use with `AudioSystem`.

**Not here:** [D32](Design/README.md#d32-sound-tables) says what was left out and why.

## Setting up a catalog

```cpp
#include "Runtime/GameSettings.h"
#include "Audio/AudioSystem.h"

namespace {
constexpr SoundId Click{0};
constexpr MusicId MenuMusic{0};
}

GameSettings Settings() {
    GameSettings settings;
    settings.applicationId = "MyProject";
    settings.audio.effects = {{Click, "Audio/click.wav",
                               {.maxConcurrent = 2, .volume = 0.8f, .overflow = EffectOverflow::Skip}}};
    settings.audio.music = {{MenuMusic, "Audio/menu.ogg", MusicVolumeGroup::Menu}};
    return settings;
}
```

The project assigns IDs; SoundId and MusicId are separate ID spaces. Sparse IDs are supported; empty slots are not
allocated up to the largest ID. A new game defines its own ID constants and catalog and supplies them in
`GameSettings::audio`. It does not need a second audio service, loading loop, or platform-specific playback code. File
paths are relative to the asset root. Runtime adds catalog paths to the asset manifest; you do not need to repeat them
in `settings.assets`. Packaging the files is the consumer's responsibility: [Assets](AssetsAndPlatforms.md).

`AudioSystem::ValidateCatalog(catalog)` checks the contract without opening a device or accessing files. An empty
catalog is valid; duplicate IDs of the same type, empty/absolute/`..` paths, zero voices, invalid volume, or invalid
enum values throw `std::invalid_argument`. SoundId and MusicId may share a numeric value. File existence and format are
checked during loading. `Game` validates the catalog before asset preparation; `BeginLoading` validates it before
starting a worker.

## Loading and ownership

`Game` creates the audio system and calls `BeginLoading`. Do not use effect playback/stop or Load/Unload APIs until
background loading completes and `FinishLoading()` succeeds. Volume values can be stored during loading and applied on
handover. File/device loading failures are reported as exceptions. `Game::Start()` may run before audio is ready; normal
scene update/input starts after loading.

With your own service, keep the waiting screen active while `FinishLoading()` is false; on success, resources are
transferred to the main thread:

| State | Behavior |
| --- | --- |
| Loading not started | `FinishLoading` and playback/resource changes throw `std::logic_error`. |
| Loading | `FinishLoading` is false; playback/resource changes throw `std::logic_error`. |
| Ready | `FinishLoading` remains true on later calls; playback and dynamic resource operations are available. |
| Loading failed | Partial resources are released; `FinishLoading` and playback/resource changes rethrow the same error. |

An instance makes one loading attempt; a second `BeginLoading` throws `std::logic_error`. If catalog validation fails,
loading has not started; fix the catalog and retry. To retry after a worker failure, destroy and recreate the service.

AudioSystem reserves the process-wide device on construction; a second live instance throws `std::logic_error`. Use the
service from the thread that created it; the public API is not a generally thread-safe service. Direct raylib audio
calls and parallel device ownership are unsupported. The loading worker uses only private resources; the main thread
takes them over after joining. The destructor requests loading cancellation and waits for the worker. Resources use
RAII; aliases are released before their source sample, and all resources before the device. References from
`Game::Audio()` and the editor must not outlive the service owner.

## Playback

- `Audio().PlayEffect(Click)`: selects an idle voice first. `EffectSettings` defaults to four concurrent copies, volume
  1, and `EffectOverflow::RestartNext`. If every copy is busy, the next restarts; `EffectOverflow::Skip` drops the new
  request. Configure each effect through catalog `EffectAsset::settings` or dynamic `LoadEffect(id, file, settings)`.
- `PlayEffectIfIdle`: drops the new request if already playing; it does not queue requests.
- `StopEffect`: stops all copies of that ID.
- `PlayMusic`: stops the current track and starts the new track at normal pitch; looping is enabled by default.
- Pitch for `PlayEffect`, `PlayEffectIfIdle`, and `SetMusicPitch` must be finite and positive; otherwise
  `std::invalid_argument` is thrown. Speed and pitch change together.
- `CurrentMusic` also reports paused or naturally finished tracks; `IsMusicPlaying` reports actual playback.
- `PauseMusic` / `ResumeMusic` pause/resume; `StopMusic` clears the current selection.
- `MusicLength` / `MusicPosition` report the current track in seconds (0 without one). `SeekMusic(seconds)` takes a
  finite, non-negative time (otherwise `std::invalid_argument`); past the end goes to the end. `SetMusicLooping`
  changes the current track's looping. All three are no-ops without a current track.
- `AudioSystem::Update()` fills the stream every frame; Game already calls it.

`HasEffect` / `HasMusic` return false before readiness. `CurrentMusic` is empty and `IsMusicPlaying` is false; with no
selected track, `StopMusic`, `PauseMusic`, `ResumeMusic`, and `Update` are safe no-ops. Other playback/resource
operations require the system to be ready first. An operation with an unloaded ID on a ready system throws
`std::out_of_range`.

`LoadEffect/LoadMusic` replace the old resource only after fully loading the new one; a failed load preserves the old
resource and playing music. Successful effect replacement applies the supplied `EffectSettings`. Successfully replacing
the playing music ID stops playback; the new stream does not start itself. A dynamic editor scene must unload only resources
it loaded itself. Game closes the device before the window.

## Tools and source files

Sound table rows appear in tools as catalog entries with their labels. Catalog paths stay relative to the asset root. `EffectAsset::name` and `MusicAsset::name` are optional labels for tools
such as the editor; playback ignores them. [AudioFiles.h](../Core/Audio/AudioFiles.h) is for tools that work on source
files: `LoadMusicFile(audio, id, file)` is `LoadMusic` for an absolute path (a relative one throws
`std::invalid_argument`), and `DecodeAudioFile(file)` returns interleaved samples without an audio device, on any
thread. Games do not need either. Command-line tools for sound files and the editor's preview: [Audio tools](AudioTools.md).

## Volume and persistence

Master, effect, music group, and per-effect volume are separate layers. Volume setters throw `std::invalid_argument` for
values outside the finite `0..1` range. Effect output is master × overall effect level × the effect's own level.
`MusicVolumeGroup::Menu` provides a separate user level for menu music. `settings.audioVolume.steps` sets percentage
steps; `AdjustEffectVolume` / `AdjustMusicVolume` adjust by these steps and return true if the level changes. The step
count must be a positive integer that divides 100; otherwise the constructor throws `std::invalid_argument`.
`EffectVolumePercent` / `MusicVolumePercent` return the percentage to display in a menu. `SetMuted` is temporary mute;
it does not change stored levels. Master and mute are not persistent preferences. The engine loads `audio.*` options and
saves them at save points; the game does not create a second settings key for them. Missing/invalid saved levels each
fall back to 100%.

## Dependencies and validation

The backend is raylib 5.5 and stays in the `Engine` target. Catalog validation and private volume policy build
headlessly in `EngineHeadless`; the public header alone does not require raylib. The engine audio infrastructure knows
no game IDs, asset paths, or playback choreography. Headless audio tests use the `[audio]` tag; actual playback and
streaming are checked manually by the developer.
