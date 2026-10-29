# Offline audio generation

`AudioGeneration/` is an opt-in Python library for authoring sound-effect WAV files offline. It is independent
of the C++ audio runtime, game catalogs and builds. The only supported import is:

```python
from AudioGeneration import SoundGenerator
```

Requirements: Python 3.10+, `python -m pip install numpy scipy`; add `soundfile` only to load non-WAV sources
(OGG, FLAC ...). Game binaries never use this library.

Paths, sound names, recipes and artistic choices belong to the caller. The library never discovers a game's
assets, updates a catalog, downloads recordings or builds anything. Keep game content out of the library; add
only general-purpose operations.

## A sound set in one file

A project keeps one script per sound set. `pack()` gives every recipe its own seeded context, applies the
finishing chain and provides the command line:

```python
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "Engine"))
from AudioGeneration import SoundGenerator

OUT = Path(__file__).resolve().parent / "Output"
PACK = SoundGenerator(44100).pack(OUT, prefix="Robot_", fade_out=0.02)


@PACK.sound("Clank", peak=0.6)
def clank(g):
    ring = g.modal(0.8, 180.0, (1.0, 2.56, 5.4, 7.9), 0.5)
    click = g.bandpass(g.noise(0.8), 2000, 9000) * g.decay(0.8, 0.006)
    return g.reverb(ring + click * 0.8, 0.9, mix=0.2)


@PACK.sound("Servo", lufs=-20)
def servo(g):
    pitch = g.contour([(0, 300), (0.4, 900), (0.6, 700)])
    whine = g.lowpass(g.saw(pitch), 2500) * g.adsr(0.6, 0.02, 0.1, 0.7, 0.15)
    return g.mix([whine, (0.55, g.thump(0.3, 220, 90, click=0.3), 0.5)])


if __name__ == "__main__":
    PACK.main()
```

```sh
python MakeRobotSounds.py                    # render every sound, print duration / peak / LUFS / warnings
python MakeRobotSounds.py Clank              # only the named sounds
python MakeRobotSounds.py --check            # render and measure, write nothing
python MakeRobotSounds.py --gallery set.html # also write a listening page for the rendered sounds
python MakeRobotSounds.py --output Tmp       # write somewhere else (relative sound folders are kept)
python MakeRobotSounds.py --list
```

- A recipe receives `g = root.for_sound(name)`: its random draws depend only on the root seed and the name, so
  rendering one sound, reordering or adding sounds never changes another sound.
- `pack(output, prefix="", **finish)` sets defaults for every sound; `@pack.sound(name, **finish)` overrides them.
  Giving a sound `lufs` clears the default `peak` (and vice versa). `output="sub/folder"` writes that sound into a
  folder relative to the pack output.
- The returned array is finished with `g.finish(...)` (below) before export, so recipes never normalize or fade
  by hand. `pack.render(name)` returns the finished samples and `pack.write(name)` writes one file.
- Root seed defaults to `0`. Change it, or a sound name, or pass `variation=` to `for_sound`, to get new random
  material.

## Conventions

- Signals are one-dimensional float NumPy arrays (mono). Times are seconds, frequencies Hz, gains linear.
- Every operation validates its inputs (finite, mono, sensible ranges) and returns a **new** array; nothing is
  normalized, clipped or faded implicitly. Sample counts use `round(seconds * sample_rate)`.
- A parameter described as a *curve* accepts a scalar or one value per output sample. Oscillators called with a
  curve take their length from it; with a scalar they need `seconds`.
- Build curves with `envelope`, `contour`, `glide`, `lfo`, `random_curve`, `decay`, `adsr`, then combine them
  with NumPy arithmetic (`pitch * (1 + 0.02 * g.lfo(1.0, 6))`).

## Operations

### Context and seeds

| Method | Notes |
| --- | --- |
| `SoundGenerator(sample_rate=44100, *, seed=0)` | `seed=None` draws a random seed that is still recorded in `seed_info` |
| `for_sound(name, *, variation=0)`, `for_layer(name, *, variation=0)` | Child context seeded by SHA-256 of (seed, kind, name, variation); never consumes the parent RNG |
| `rng`, `seed`, `seed_info` | The context's NumPy generator; integer seed; `{"seed", "scopes"}` |
| `sample_count(seconds)`, `time_axis(seconds)`, `silence(seconds)` | |
| `note(value)` | Hz of a MIDI number or `"A4"`, `"C#5"`, `"Eb3"` (static) |

### Control curves

| Method | Notes |
| --- | --- |
| `envelope(points, seconds=None, *, curve=1)` | `[(t, value), ...]`; holds the first/last value outside the points; `curve < 1` moves fast first, `> 1` slow first; repeated times make a jump |
| `contour(points, seconds=None)` | Same breakpoints, interpolated geometrically: pitch, cutoff, ratios |
| `adsr(seconds, attack, decay, sustain, release, *, curve=1)` | Ends at zero at `seconds`; stages shrink proportionally if they do not fit |
| `decay(seconds, time_constant, *, attack=0)` | Linear attack then `exp(-t / time_constant)` |
| `glide(start, end, seconds, time_constant)` | Exponential approach (pitch drops, closing filters) |
| `lfo(seconds, rate, *, shape="sine", phase=0)` | `sine`, `triangle`, `square`, `saw` in [-1, 1]; `rate` may be a curve |
| `random_curve(seconds, rate, *, low=-1, high=1)` | Smooth random targets, `rate` per second, never outside [low, high] |
| `fade(samples, fade_in=0, fade_out=0, *, curve=1)` | Edge ramps that reach exactly zero |

### Sources

| Method | Notes |
| --- | --- |
| `phase(frequency, seconds=None, *, start=0)` | Radians, beginning exactly at `start` |
| `sine(frequency, seconds=None, *, phase=0)` | |
| `saw`, `square`, `triangle(frequency, seconds=None, *, phase=0, max_harmonics=512)` | Band-limited additive (see below) |
| `pulse(frequency, seconds=None, *, width=0.5, ...)` | Band-limited, DC-free; `width` is the high part of each cycle |
| `fm(frequency, seconds=None, *, ratio=1, index=1, phase=0)` | `sin(carrier + index * sin(ratio * carrier))`; index/ratio may be curves (bells, metal, growls) |
| `noise(seconds, *, color="white")` | `white` (uniform), `pink` (1/f), `brown` (1/f²); peak 1 |
| `modal(seconds, frequency, ratios, decay, *, gains=None, falloff=0.5, attack=0.0005, random_phase=True)` | Struck resonator bank: partial *i* at `frequency * ratios[i]`; `decay` per partial or one value divided by `1 + falloff * i`; gains default `1 / (i + 1)`; partials above Nyquist are skipped. Use `frequency=1` to give absolute Hz in `ratios` |
| `thump(seconds, start, end, *, sweep=0.03, decay=0.15, click=0)` | Pitch-dropping sine hit (kick, heartbeat, impact) with optional noise click |
| `pluck(frequency, seconds, *, decay=1, brightness=0.6)` | Karplus-Strong string; `decay` is time to -60 dB; pitch is quantized to whole delay samples |
| `scatter(seconds, density, grain, *, gain=None)` | `round(density * seconds)` copies of `grain` (array or zero-argument callable) at random onsets; `gain` is a per-sample curve read at each onset; grains crossing the end are cropped |

The additive oscillators fade each partial out between 80% and 95% of Nyquist, so pitch sweeps never switch
partials abruptly and nothing folds back. Fast FM/AM can still create sidebands; that is the modulation, not the
oscillator. Lower `max_harmonics` for a deliberately duller tone.

### Filters

| Method | Notes |
| --- | --- |
| `lowpass(samples, cutoff, *, order=2)`, `highpass(...)` | Fixed cutoff: Butterworth (SOS). Cutoff curve: cascaded state-variable stages with Butterworth Qs (even orders) |
| `bandpass(samples, low, high, *, order=2)` | Fixed edges: Butterworth band-pass. Curve edges: `order` unity-peak resonant stages tracking center and width |
| `resonate(samples, center, *, q=4)` | Unity-peak resonant band; center and q may be curves |
| `formants(samples, bands)` | Sum of `(center Hz or curve, q, gain)` bands: vowels, growls, roars |
| `dc_block(samples, cutoff=20)` | First-order high-pass |

Moving cutoffs use a trapezoidal state-variable filter (Andrew Simper, Cytomic,
[SvfLinearTrapOptimised2](https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf)) updated every sample. The
cutoff is smoothed in log-Hz with a 5 ms time constant, so even stepped curves sweep without clicks. Each call
starts with silent filter state; filter a whole clip in one call.

### Effects

All wet/dry effects use `dry * (1 - mix) + wet * mix`. Effects that ring on (`delay`, `reverb`) return the
input length plus their tail.

| Method | Notes |
| --- | --- |
| `saturate(samples, drive, *, oversample=4)` | `tanh(x * drive) / tanh(drive)` (input 1 stays 1) at 1/2/4/8/16x with a Kaiser FIR; length and alignment unchanged; drive 0 bypasses |
| `bitcrush(samples, *, bits=None, rate=None)` | Quantize and/or sample-and-hold (arcade, radio, broken machines) |
| `delay(samples, time, *, feedback=0.4, mix=0.35, damp=None)` | Repeats every `time`, each `feedback` quieter; `damp` low-passes each repeat; stops at -60 dB |
| `chorus(samples, *, rate=0.8, depth=0.003, delay=0.015, mix=0.5)` | Two modulated delay voices; short delay/depth gives flanging |
| `reverb(samples, seconds=1.6, *, mix=0.25, damp=6000, predelay=0.012)` | Noise-impulse room with unit energy: a steady input keeps its RMS in the wet signal |
| `speed(samples, factor)` | Tape-style resample: factor 2 is an octave up and half as long |
| `follow(samples, *, attack=0.005, release=0.08)` | Envelope follower, for ducking or driving another parameter |

### Timeline

| Method | Notes |
| --- | --- |
| `mix(layers, *, seconds=None)` | List or `{name: ...}` of arrays, `(at, samples)` / `(at, samples, gain)` tuples or `layer` values; length is the last layer end unless `seconds` pads/crops; negative starts are rejected |
| `layer(samples, *, at=0, gain=1)` | Read-only snapshot with placement and the context's seed metadata (for `review_clip` stems) |
| `pad(samples, seconds)`, `segment(samples, seconds, start=0)` | Append silence; slice with zero padding past the end |

### Level and export

| Method | Notes |
| --- | --- |
| `normalize(samples, *, peak=None, lufs=None)` | Exactly one target; silence stays silent |
| `loudness(samples)` | Integrated loudness, mono ITU-R BS.1770 (K-weighting, 400 ms blocks, absolute -70 and relative -10 LU gates); clips shorter than 400 ms are measured whole; `None` for silence |
| `trim(samples, *, threshold_dbfs=-60, window=0.01, hold=0.05, fade=0.01, minimum=0)` | Cut after the last window whose RMS (or peak 12 dB lower) is above the threshold; earlier gaps and late reflections are kept |
| `finish(samples, *, peak=0.9, lufs=None, ceiling=0.98, dc=True, fade_in=0.0005, fade_out=0.01, trim=-60)` | Export chain below |
| `load(path)` | WAV via SciPy, other formats via soundfile; channel mean, resampled to the context rate; DC and level untouched |
| `write_wav(path, samples)` | Mono PCM16, rounded and clipped; creates folders, overwrites; rejects NaN/Inf; returns the path |
| `pack(output, *, prefix="", **finish)` | Sound set, see above |

`finish` runs: DC block (`dc=True` is 20 Hz, a number sets the cutoff, `False` skips) → level to `peak` or
`lufs` → `trim` (threshold in dBFS of that level; `None` keeps the length) → edge fades → level again so the
target is exact → scale down to `ceiling` if the peak exceeds it. Use `peak=None, lufs=None` to keep the
recipe's own level. Keep `trim=None` for sounds whose length is synchronized with the game (tempo, cinematic
timings).

Loudness is the better tool for making a set consistent by ear; peak is better for transients that must not
exceed a level. LUFS here is an integrated measurement for authoring, not a true-peak meter or a mastering chain.

## Inspection

| Method | Notes |
| --- | --- |
| `analyze(samples, *, silence_dbfs=-60, frame_seconds=0.02, edge_dbfs=-40)` | JSON-safe report; never modifies the signal |
| `format_report(report)`, `write_report(path, report)` | Text summary; `.json` + `.txt` siblings |
| `export_gallery(path, sounds, *, title="Sound gallery")` | One self-contained HTML page for `{name: samples}`: player, waveform, spectrogram, peak/RMS/LUFS and warnings; sounds play at their true relative level; "Play all" |
| `review_clip(source=None, *, layers=None, seconds=None, parameters=None)` | Snapshot an array, file or named layers with seed and JSON parameters |
| `compare(reference, candidate, *, focus="all", ...)` | Active-region and attack/body/tail differences against a reference |
| `export_review(directory, reference, variants, *, ...)` | Offline A/B listening session against a reference recording |

`analyze` reports format, duration, peak/RMS dBFS, crest factor, integrated loudness, DC offset, NaN/Inf counts,
samples beyond full scale, active time and leading/trailing silence, edge amplitudes and the largest sample step,
whole-signal and per-frame spectral centroid/flatness and band energy (sub < 80 Hz, bass < 250 Hz, mid < 4 kHz,
treble), plus warnings (`clipping`, `non_finite`, `empty`, `silent`, `nonzero_start`, `nonzero_end`). Edge
warnings describe a possible click, not a verified one. Undefined values are JSON `null`; reports carry
`schema_version: 1`. These are structural measurements, not a quality score.

### Reference review

Use `export_review` when a reference recording exists and you want to match it:

```python
g = SoundGenerator(48000, seed=42)
sound = g.for_sound("Impact")
body, air = sound.for_layer("body"), sound.for_layer("air")
candidate = sound.review_clip(
    layers={"body": body.layer(body.thump(0.5, 160, 60), gain=0.6),
            "air": air.layer(air.bandpass(air.noise(0.1), 800, 4000) * air.decay(0.1, 0.02), gain=0.2)},
    parameters={"start_hz": 160, "end_hz": 60})
reference = g.review_clip(Path("References") / "MetalImpact.wav")
print(g.format_report(g.compare(reference, candidate, focus="attack")))
paths = g.export_review(Path("Output") / "ImpactReview", reference, {"Heavy": candidate}, target_dbfs=-20)
```

Runnable example (no recordings needed), from the Engine root:
`python -B AudioGeneration/Examples/reference_review.py --output MyAudioReview`.

- **Alignment:** onsets come from a 1 ms trailing RMS envelope, thresholded at the larger of `silence_dbfs`
  (absolute) and `onset_relative_db` below the clip's peak. `align` only offsets playback; samples are never
  shifted or stretched.
- **Stages:** `all` is the active region; `attack` runs to the first 90% crossing of the dominant envelope peak;
  `body` runs until 90% of active energy; `tail` is the rest. Stages may be empty (reported as `null`).
- **Differences** are candidate minus reference for duration, peak/RMS, crest, DC, centroid, flatness and band
  energy (percentage points), plus rise and time-to-peak for `all`. Findings flag at least 5 ms, 0.5 dB, 50 Hz,
  0.05 flatness, 0.01 DC or 5 points: scanning aids, not pass/fail thresholds.
- **Listening page:** A/B switching that keeps the position, loop, seek, onset alignment, active-RMS or peak
  matching to `target_dbfs` (boost capped by `max_gain_db`, every clip and stem kept under a 0.89 sample-peak
  ceiling), original-balance playback, per-clip fine gain, layer solo, waveform/spectrogram plots and a "Save
  variant selection" JSON download. Keys: Space play/pause, A/B, arrows seek 100 ms, L loop.
- **Files:** `index.html` (self-contained), `review.json`/`review.txt`, `raw/` float32 WAV snapshots,
  `audio/` PCM16 previews. Output that would overwrite a source recording is rejected; other existing session
  files are overwritten. Nothing is fetched or uploaded.

## Structure and extension

```text
AudioGeneration/
    __init__.py            supported import: SoundGenerator
    generator.py           public facade: sample rate, seed and every operation
    contracts.py           returned data values (AudioLayer, AudioReviewClip)
    Internals/
        validation.py      shared input contracts (mono, curves, Hz ranges)
        seeding.py         named seed derivation
        curves.py          envelopes, contours, LFOs, random curves, fades
        oscillators.py     band-limited waves, FM, noise colors, modal, thump, pluck
        filters.py         Butterworth and per-sample state-variable filters, formants
        effects.py         saturation, bitcrush, delay, chorus, reverb, speed, follower
        timeline.py        layers, mix, pad, segment, scatter
        level.py           normalize, BS.1770 loudness, trim, finish
        wave_io.py         PCM16 encoding/writing, source loading
        pack.py            SoundPack: recipes, finishing, command line, summaries
        gallery.py         gallery export (+ gallery_page.html)
        analysis.py        measurements; reports.py text/JSON reports
        comparison.py      reference landmarks, stages and differences
        review*.py         review clips, plots and A/B session export (+ review_page.*)
    Tests/                 unittest suites + review_player.test.cjs (Node, in-memory DOM)
    Examples/              reference_review.py
```

New operations go on `SoundGenerator` with the implementation in the matching internal module (or a new one);
validate inputs with `Internals/validation.py`, return new arrays and add a public-API test. Consumers never
import `Internals`. Recipes that are specific to one game stay in that game's scripts.

## Tests

From the Engine root:

```sh
python -B -m unittest discover -s AudioGeneration/Tests
node --test AudioGeneration/Tests/review_player.test.cjs
```

The Python tests use temporary files and only the public API (NumPy and SciPy required). The Node tests check
the review player's transport, gain, solo and saved selections in an in-memory DOM; they do not replace a
real-browser listen.
