# Audio

The engine owns the audio device and resources; the game decides what plays and when. A game's sounds belong in
[sound tables](#sound-tables), played through `game.Sounds()`: one-shots, [sounds in the world](#sounds-in-the-world),
[live sounds](#live-sounds) a view keeps changing, and [layers](#layers). Music, and any effect a game prefers to number by
hand, goes in a [catalog](#setting-up-a-catalog) played by [AudioSystem.h](../Core/Audio/AudioSystem.h).

The mix is miniaudio's engine, with the engine's own stops on its way: every sound goes through a voice, a
[bus](#buses) and the master, and each of those has a gain per ear, a volume with fades and a low-pass filter. It runs on
the audio thread: fades, loops and streams go on while the main thread is busy, so a long frame or a seek does not break
a live sound. Public headers include neither miniaudio nor raylib; `Audio/Internals/` is not supported API, and `check`
keeps the audio library there. Why miniaudio: [D39](Design/README.md#d39-spatial-and-layered-audio).

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
services.sounds.Play(Sfx::Hit); // In a scene, from its SceneServices; game.Sounds() is the same player
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

`IsPlaying(key)` asks whether any file of a row is playing, for "never over itself" when the row itself allows copies. A game's menus use its rows through `settings.uiSounds`: [UI sounds](Ui.md#sounds).

**Not here:** [D32](Design/README.md#d32-sound-tables) says what was left out and why.

### Row options for what goes on

| Option | Default | Meaning |
| --- | --- | --- |
| `loop` | false | Plays until stopped. Such a row only starts with `Start`, which hands back the sound: `Play` throws, since nothing could stop it |
| `streamed` | false | Read from disk while it plays instead of held in memory. For ambience and other long files; a decoded minute of 48 kHz stereo is 23 MB |
| `bus` | `SoundBus::World` | Where it is mixed: `World`, `Ui` or `Music` ([buses](#buses)) |
| `falloff` | 160 to 1280 units, `InQuad` | How it fades with distance when played at a place ([sounds in the world](#sounds-in-the-world)) |

`Play(key, {...})` takes a `SoundPlay`: `volume`, `pitch` (times the row's), `at` (a place in the world), `pan` (for a
sound that is not in the world), `muffle`, `fadeIn` and `startAt` (seconds into the file).

## Sounds in the world

```cpp
sounds.SetListener({.position = view.target, .rotation = view.rotation, .panWidth = view.VisibleArea().width / 2});
sounds.Play(Sfx::Door, {.at = door});           // Pan and distance from the listener
```

A `SoundListener` is plain data: a position, a rotation (radians, clockwise, as `CameraView`), `panWidth` (world units to
the side at which a sound reaches `maxPan`), `maxPan` (0.8, so a sound far to one side is never silent in the other ear)
and `distanceScale` (below 1 hears further, for a zoomed-out view). A camera feeds it as above, or a player's character
does; nothing in `Audio` knows the camera. Before the first call one listener stands at the origin. Split screen:
`SetListeners`, and for each sound the listener who hears it best decides.

A row's `SoundFalloff` is full within `fullWithin`, silent beyond `silentBeyond`, and its `curve` between, read from far
(0) to near (1). Pan is constant power: in the middle both ears get 1, at one side that ear gets √2, so a sound moving
across keeps its loudness. [SoundSpace.h](../Core/Audio/SoundSpace.h) holds the math (`PlaceSound`, `FalloffGain`,
`PanEars`), header-only, so a test or a game can ask the same question. Moving the listener moves every live sound at
once.

## Live sounds

```cpp
LiveSound _hum = sounds.Start(Noise::Hum, {.at = ghost});   // A member: it lives as long as the handle
_hum.SetPosition(ghost);                                   // Every frame, from the state
_hum.SetPitch(1.0f + closeness);
_hum.SetVolume(closeness, 0.2f);                           // Seconds of fade
_hum.SetMuffle(behindWall ? 0.9f : 0.0f, 0.15f);
```

`Start` begins a row and returns a `LiveSound`. Cooldown and overlap rules do not apply to it, and it does not count
towards a row's copies. Every change glides, however often it comes: a value set every frame does not click, and `seconds`
lengthens the glide. `SetPan` takes it out of the world again. When the handle goes away, or at `Stop(fade)`, the sound
fades out and ends. A row without `.loop` ends by itself, after which the handle reports `Playing() == false` and every
call does nothing; a handle may outlive the player. Invalid values throw `std::invalid_argument` whether or not anything
plays. `StartTogether(keys)` starts several rows on the same sample, for files that must stay in step.

## Layers

```cpp
SoundLayers ambience(sounds, 2.0f);                        // Default fade in seconds
ambience.Set(Ambience::Wind, 0.8f);
ambience.Mix({{Ambience::Rain, 0.8f}, {Ambience::Drip, 0.3f}}); // Exactly this; the others fade to 0
ambience.Silence();                                        // And Restore(): the mix is remembered
ambience.SetMuffle(0.8f);                                  // All layers, behind a door
```

A layer is a row, started silent the first time it is named and kept running afterwards, so it never jumps back to its
start. Fades cross at constant power: two layers trading places do not dip. `Prepare(keys)` starts stems sample-aligned.
`Stop` ends them; going away does too. Nothing needs calling every frame.

## Buses

`SoundBus::World` is what happens in the game, `Ui` is menus, `Music` is music and music-like layers. The effects volume
option scales World and Ui, the music options scale Music. On top of that, each bus has a level and muffling a game sets
on purpose, under the options and without changing them:

```cpp
sounds.FadeBus(SoundBus::World, 0.25f, 0.3f);              // A pause menu lowers the world
sounds.MuffleBus(SoundBus::World, 0.7f, 0.3f);             // And muffles it
```

`muffle` is a low-pass filter from 20 kHz at 0 to 300 Hz at 1, moving on a logarithmic scale; it can change while a sound
plays without a click.

## Pausing and catching up

`Game` pauses the live sounds of `SoundBus::World` while the game's time is paused, and lets them go on from the same
place; menus and music keep playing. One-shots already playing finish. The editor's pause, and every seek, hold all live
sounds the same way. Sounds are view code: a simulation never starts one, so its state hash and replays do not change,
and a catch-up of many steps leaves live sounds alone: they go on in real time on the audio thread. Events a seek passes
over are silent (N1, `SilenceEventsUntil`).

## Ready-made sounds

The engine ships menu sounds in `Assets/Audio/Ui`. A game names them under `Engine/`, with no file to copy:

```cpp
SoundRow{Sfx::UiMove, {"Engine/Audio/Ui/UiMove_Pop.wav"}},
SoundRow{Sfx::UiConfirm, {"Engine/Audio/Ui/UiConfirm.wav"}},
SoundRow{Sfx::UiBack, {"Engine/Audio/Ui/UiBack.wav"}},
```

The others are `UiMove_Blip.wav` and `UiMove_Wood.wav`. Every build copies them beside the game, and `release` packages
only those the game uses ([Release](Release.md#only-the-sounds-a-game-uses)). The Assets panel lists them under
Engine/Audio. The confirm and back sounds come from
[`ui_confirm_back_sounds.py`](../Tools/AudioGeneration/Examples/ui_confirm_back_sounds.py).

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
handover. File/device loading failures are reported as exceptions. `Game::Start()` runs before audio is ready; the first
scene starts after loading, so its `Start` and `Enter` can play. A [loading screen](Runtime.md#startup-loading-and-error-boundary)
may play once its `Loaded()` is true.

With your own service, keep the waiting screen active while `FinishLoading()` is false:

| State | Behavior |
| --- | --- |
| Loading not started | `FinishLoading` and playback/resource changes throw `std::logic_error`. |
| Loading | `FinishLoading` is false; playback/resource changes throw `std::logic_error`. |
| Ready | `FinishLoading` remains true on later calls; playback and dynamic resource operations are available. |
| Loading failed | Partial resources are released; `FinishLoading` and playback/resource changes rethrow the same error. |

An instance makes one loading attempt; a second `BeginLoading` throws `std::logic_error`. If catalog validation fails,
loading has not started; fix the catalog and retry. To retry after a worker failure, destroy and recreate the service.

Use the service from the thread that created it; the public API is not a thread-safe service. The loading worker owns
the device and resources until `FinishLoading` joins it. The destructor requests loading cancellation and waits for the
worker; every voice goes before the device. References from `Game::Audio()` and the editor must not outlive the service
owner. Each `AudioSystem` opens its own device, so a tool may hold one beside a game's.

## Playback

- `Audio().PlayEffect(Click)`: `EffectSettings` defaults to four concurrent copies, volume 1, and
  `EffectOverflow::RestartNext`. If every copy is busy, the oldest fades out in a few milliseconds beside the new one;
  `EffectOverflow::Skip` drops the new request. `bus` and `streamed` mean what the row options do. Configure each effect
  through catalog `EffectAsset::settings` or dynamic `LoadEffect(id, file, settings)`.
- `PlayEffectIfIdle`: drops the new request if already playing; it does not queue requests.
- `StopEffect`: stops all copies of that ID.
- `PlayMusic(id, {.loop, .fadeSeconds})`: starts the track at normal pitch, from the top, looping by default, and ends
  the current one, at once or crossing into the new one over `fadeSeconds`. `StopMusic(fadeSeconds)` fades too.
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
the playing music ID stops playback; the new stream does not start itself. A scene that loads audio itself must unload only resources
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

## Formats

WAV, OGG (Vorbis), FLAC and MP3 load, decoded or streamed. Every voice keeps its file's own sample rate and resamples as it
plays, so a loop has no seam whatever the file's rate. For loops prefer OGG, FLAC or WAV: MP3 adds padding at the start
and end of a file in most encoders, which a loop may make audible (one test file showed none; a file from another encoder
can). `.qoa`, `.xm` and `.mod`, which raylib read, do not load.

## Checking by numbers

Nobody can hear in a test. `AudioSystem(root, {}, AudioOutput::Offline)` opens no device: `Render(stereo)` hands out the
next frames of the real mix, so a test measures pan, fades, seams and muffling on what a device would have played
(`Core/Tests/Audio`). In a running game or the editor, `StartCapture(file, seconds)` writes what the device plays to a
WAV file; `python Engine ctl audio capture SECONDS` does it for the open scene ([Audio tools](AudioTools.md)).

## Dependencies and validation

The backend is miniaudio 0.11.26 with stb_vorbis, in the `EngineAudio` target, which needs no window; `Engine` links it.
raylib's own audio module is left out of the build. `SoundPlayer`, `LiveSound`, `SoundLayers` and the volume policy build
in `EngineHeadless` and reach the device only through `SoundOutput`, so their decisions are tested with a fake. The
engine audio knows no game IDs, asset paths, or playback choreography. Audio tests use the `[audio]` tag.
