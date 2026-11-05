# Audio tools

Tools and agents cannot listen, so the engine turns sound files into numbers and pictures, and plays a sound for the
developer on request. Two commands, run from the project root:

- `python Engine audio ...` reads sound files and the project's audio catalog. It needs no open editor.
- `python Engine ctl audio ...` drives the open editor's Audio Preview panel ([Editor automation](Editor/Automation.md)).

Both print one JSON object. Files are named absolutely or relative to the current folder, the project root or the
asset folder (`Audio/hit.ogg`).

## `audio`: files and catalog

The project's editor worker does the decoding (`<worker> --audio-report REQUEST.json`). It writes an answer file and
exits, without opening a window or an audio device. Build it once with `python Engine editor --build-only`. The catalog
comes from that build, so after changing the catalog in code, build again (or use Build & Refresh in the editor). With
no worker built, the command exits 2 and says how to build it.

The worker knows only the settings its own `Editor/Main.cpp` passes ([Editor/Main.cpp](Editor/GettingStarted.md#editormaincpp)).
A project without one runs on the engine's starter worker, which knows none of the game's sound tables: `info`, `usage`
and `catalog` then add a `note` saying so, because those sounds show no uses. A release is not affected; it keeps the
sounds the built game names ([Release](Release.md#only-the-sounds-a-game-uses)).

| Command | Answer |
| --- | --- |
| `audio info FILE... [--silence-db DB]` | `files`: per file `file`, `asset` (path in the asset folder; null outside it), `seconds`, `sampleRate`, `channels`, `frames`, `peakDb`, `rmsDb`, `leadingSilence`, `trailingSilence`, `clipped`, `uses`; or `file` and `error`. Exit 1 when any file fails |
| `audio waveform FILE [--out PNG] [--width W] [--height H]` | Draws the waveform (default 1200×240) to `path`, by default `audio-report/<name>-waveform.png` in the editor build folder; also `width`, `height`, `seconds`, `tickSeconds` |
| `audio usage [FOLDER] [--unused \| --missing]` | `sounds` (each `asset` and its `uses`), `unused` (how many no entry plays) and `missing` (catalog entries whose file does not exist). `--unused`: only the unused paths; `--missing`: only the missing entries. FOLDER limits the scan to part of the asset folder |
| `audio catalog` | `uses`: every catalog entry with `kind` (`effect` / `music`), `id`, `name`, `file`, `label` |

Reading the numbers:

- Levels are dBFS: 0 is full scale, `-120` stands for silence. `peakDb` is the loudest sample and `rmsDb` the average
  loudness. Two sounds played side by side compare by `rmsDb`. A peak at 0 together with `clipped` > 0 means the file
  distorts.
- `leadingSilence` / `trailingSilence` are seconds quieter than `--silence-db` (default -60) at either end. A short
  effect with a long leading silence feels late; a long tail wastes a voice.
- A `uses` label is `Effect <name>` / `Music <name>`, or `#<id>` when the catalog entry has no `name`. Names are optional
  `EffectAsset::name` / `MusicAsset::name` labels ([Audio](Audio.md#tools-and-source-files)). A [sound table](Audio.md#sound-tables) row
  shows as one entry per file, with its label, and IDs from 2^32 up.
- In the waveform image, the bright core is RMS, the darker edge the peak, and faint vertical lines mark every
  `tickSeconds`; x maps linearly to time over `seconds`.

## `ctl audio`: playing for the developer

`ctl audio play FILE` loads a sound into Audio Preview and plays it in the editor process, from its source file, so a
new file needs no Build & Refresh; the running scene is not touched. `ctl audio` reports what is loaded, the position,
and the catalog entries that play it. Other actions: `pause`, `resume`, `stop`, `seek S`, `volume V` (0–1),
`loop on|off`. All of them except plain `ctl audio` need Agent control on.

`ctl audio capture SECONDS [--out WAV]` records what the open scene plays, through the real device, to a 32-bit float
stereo WAV (by default `audio-report/capture.wav` in the editor build), waits until it is written and prints `path`.
`audio info` and `audio waveform` then read it like any file: loudness per channel, clipping, silences. It needs a
running scene and Agent control on.

Use it to let the developer hear a sound you changed or picked, then ask what they think; your own judgement comes
from `audio info` and `audio waveform`.
