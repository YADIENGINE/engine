# Audio

The engine owns the audio device and resources; the game decides what plays and when. The single public entry point is
[AudioSystem.h](../Core/Audio/AudioSystem.h): the service, IDs, catalog, and settings are in this header. The header
includes neither raylib nor the loading thread. `Audio/Internals/` is not supported API; games and editors use only the
public header. Audio is an engine runtime service, not a separate opt-in module.

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

Catalog paths stay relative to the asset root. `EffectAsset::name` and `MusicAsset::name` are optional labels for tools
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
