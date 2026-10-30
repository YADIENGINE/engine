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
    ring = g.modal(0.8, 180.0, (1.0, 2.56, 5.4, 7.9), 0.5, click=0.8)
    return g.reverb(ring, 0.9, mix=0.2)


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
python MakeRobotSounds.py --list             # sounds and their tweakable parameters

# Listen before overwriting: these never write audio files.
python MakeRobotSounds.py --compare --gallery cmp.html                    # new take vs the file on disk (A/B)
python MakeRobotSounds.py Clank --variations 4 --gallery pick.html       # seed variations 0..3
python MakeRobotSounds.py Servo --set top=700,900,1200 --gallery pick.html   # one take per parameter value
```

`--variations` and `--set` combine (every variation for every value; repeat `--set` for several parameters).
`--set` reaches a recipe's keyword parameters that have defaults (`def servo(g, top=900): ...`), and values
parse as int, float or text. In the gallery, "Pick" one take per sound and "Download selection" saves
`selection.json` (`{"sound", "variation", "parameters"}` per pick); apply it by setting
`@PACK.sound(..., variation=N)` and the parameter defaults in the recipe.

- A recipe receives `g = root.for_sound(name)`: its random draws depend only on the root seed and the name, so
  rendering one sound, reordering or adding sounds never changes another sound.
- `pack(output, prefix="", format="wav", bits=16, dither=False, **finish)` sets the file type (`wav`, `flac`,
  `ogg`; see `save`) and finishing defaults; `@pack.sound(name, **finish, output=None, variation=0)` overrides
  them per sound. Giving a sound `lufs` clears the default `peak` (and vice versa). `output="sub/folder"` writes
  that sound into a folder relative to the pack output; `variation` picks a different seed for that sound.
- The returned array is finished with `g.finish(...)` (below) before export, so recipes never normalize or fade
  by hand. `pack.render(name, variation=None, parameters=None)` returns the finished samples,
  `pack.write(name)` writes one file and `pack.parameters(name)` lists a recipe's keyword defaults.
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
| `decay(seconds, time_constant, *, attack=0, hold=0)` | Linear rise over `attack`, full level for `hold`, then `exp(-t / time_constant)` |
| `rise(seconds, time, *, curve=1)` | 0 to 1 over `time`, then 1 (fade-ins, swells, opening filters) |
| `glide(start, end, seconds, time_constant)` | Exponential approach (pitch drops, closing filters) |
| `lfo(seconds, rate, *, shape="sine", phase=0, center=0, depth=1)` | `center + depth * wave` with `sine`, `triangle`, `square` or `saw` in [-1, 1]; `rate` may be a curve. Vibrato: `pitch * g.lfo(s, 6, center=1, depth=0.02)` |
| `random_curve(seconds, rate, *, low=-1, high=1)` | Smooth random targets, `rate` per second, never outside [low, high] |
| `fade(samples, fade_in=0, fade_out=0, *, curve=1)` | Edge ramps that reach exactly zero |

### Sources

| Method | Notes |
| --- | --- |
| `phase(frequency, seconds=None, *, start=0)` | Radians, beginning exactly at `start` |
| `sine(frequency, seconds=None, *, phase=0)` | |
| `saw`, `square`, `triangle(frequency, seconds=None, *, phase=0, max_harmonics=512)` | Band-limited additive (see below) |
| `pulse(frequency, seconds=None, *, width=0.5, ...)` | Band-limited, DC-free; `width` is the high part of each cycle |
| `unison(frequency, seconds=None, *, wave="saw", voices=7, detune=20, max_harmonics=512)` | Detuned stack (supersaw): voices spread evenly over +/- `detune` cents with random phases, divided by sqrt(voices); `wave` is `sine`, `saw`, `square` or `triangle` |
| `fm(frequency, seconds=None, *, ratio=1, index=1, phase=0)` | `sin(carrier + index * sin(ratio * carrier))`; index/ratio may be curves (bells, metal, growls) |
| `noise(seconds, *, color="white")` | `white` (uniform), `pink` (1/f), `brown` (1/f²); peak 1 |
| `modal(seconds, frequency, ratios, decay, *, gains=None, falloff=0.5, attack=0.0005, random_phase=True, spread=0, click=0)` | Struck resonator bank: partial *i* at `frequency * ratios[i]`; `decay` per partial or one value divided by `1 + falloff * i`; gains default `1 / (i + 1)`; partials above Nyquist are skipped. `spread` detunes every partial randomly by up to that fraction (each strike differs); `click` adds 6 ms contact noise above 2 kHz. Use `frequency=1` to give absolute Hz in `ratios` |
| `thump(seconds, start, end, *, sweep=0.03, decay=0.15, click=0)` | Pitch-dropping sine hit (kick, heartbeat, impact) with optional noise click |
| `pluck(frequency, seconds, *, decay=1, brightness=0.6)` | Karplus-Strong string; `decay` is time to -60 dB; a first-order all-pass supplies the fractional delay, so high notes stay in tune (20 Hz to a quarter of the sample rate) |
| `burst(seconds, decay, *, band=None, lowpass=None, highpass=None, attack=0, color="white")` | Noise through at most one filter times `decay(seconds, decay, attack=...)`: clicks, hits, breath, rumble tails. `band` edges may be curves |
| `crackle(seconds, grains, band, *, within=None, length=(0.004, 0.012), decay=0.003, gain=(0.3, 1), envelope=None)` | `grains` short noise grains, band-passed: fire, ice, gravel, sparks. `within` is `(start, end)` seconds or a zero-argument callable returning an onset (for skewed timing); `length`/`gain` are per-grain ranges; `envelope` is read at each onset |
| `scatter(seconds, density, grain, *, gain=None)` | `round(density * seconds)` copies of `grain` (array or zero-argument callable) at random onsets; `gain` is a per-sample curve read at each onset; grains crossing the end are cropped |

The additive oscillators fade each partial out between 80% and 95% of Nyquist, so pitch sweeps never switch
partials abruptly and nothing folds back. Fast FM/AM can still create sidebands; that is the modulation, not the
oscillator. Lower `max_harmonics` for a deliberately duller tone.

### Filters

| Method | Notes |
| --- | --- |
| `lowpass(samples, cutoff, *, order=2)`, `highpass(...)` | Fixed cutoff: Butterworth (SOS). Cutoff curve: cascaded state-variable stages with Butterworth Qs (even orders) |
| `bandpass(samples, low=None, high=None, *, center=None, width=None, order=2)` | Edges, or `center` with a `width` ratio above 1 (`center / width` .. `center * width`); any may be curves. Fixed edges: Butterworth band-pass. Curve edges: `order` unity-peak resonant stages tracking center and width |
| `resonate(samples, center, *, q=4)` | Unity-peak resonant band; center and q may be curves |
| `formants(samples, bands)` | Sum of `(center Hz or curve, q, gain)` bands: vowels, growls, roars |
| `dc_block(samples, cutoff=20)` | First-order high-pass |
| `bell(samples, frequency, gain_db, *, q=1)` | Peaking EQ (RBJ cookbook biquad); higher q is narrower |
| `low_shelf(samples, frequency, gain_db, *, q=0.707)`, `high_shelf(...)` | Boost/cut below or above `frequency` |
| `vowel(samples, shape, *, scale=1)` | F1-F3 formants of `"a"`, `"e"`, `"i"`, `"o"`, `"u"`, or `[(seconds, vowel), ...]` gliding in log-Hz; `scale < 1` sounds like a larger throat |

Moving cutoffs use a trapezoidal state-variable filter (Andrew Simper, Cytomic,
[SvfLinearTrapOptimised2](https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf)) updated every sample. The
cutoff is smoothed in log-Hz with a 5 ms time constant, so even stepped curves sweep without clicks. Each call
starts with silent filter state; filter a whole clip in one call.

### Effects

All wet/dry effects use `dry * (1 - mix) + wet * mix`. Effects that ring on (`delay`, `reverb`) return the
input length plus their tail.

| Method | Notes |
| --- | --- |
| `saturate(samples, drive, *, shape="tanh", bias=0, oversample=4)` | Waveshaper run at 1/2/4/8/16x with a Kaiser FIR (length and alignment unchanged): `tanh` soft, `clip` hard, `fold` sine wavefolder; `bias` makes it asymmetric (even harmonics, silence stays silent); inputs in [-1, 1] stay in [-1, 1] (symmetric tanh/clip map 1 to 1); drive 0 bypasses |
| `bitcrush(samples, *, bits=None, rate=None)` | Quantize and/or sample-and-hold (arcade, radio, broken machines) |
| `delay(samples, time, *, feedback=0.4, mix=0.35, damp=None)` | Repeats every `time`, each `feedback` quieter; `damp` low-passes each repeat; stops at -60 dB |
| `chorus(samples, *, rate=0.8, depth=0.003, delay=0.015, mix=0.5)` | Two modulated delay voices; short delay/depth gives flanging |
| `reverb(samples, seconds=1.6, *, mix=0.25, damp=6000, predelay=0.012, impulse=None)` | Unit-energy convolution: a steady input keeps its RMS in the wet signal. The synthetic room decays each octave band over `seconds / (1 + (center / damp)^2)`, so highs die first; `impulse` (samples or a file) uses a recorded response instead |
| `speed(samples, factor)` | Tape-style resample: factor 2 is an octave up and half as long |
| `follow(samples, *, attack=0.005, release=0.08)` | Envelope follower, for ducking or driving another parameter |
| `stretch(samples, factor)` | WSOLA time stretch: duration × factor, pitch unchanged (40 ms frames, ±12 ms similarity search) |
| `pitch_shift(samples, semitones)` | Pitch change with the same duration (stretch, then resample); best on tonal or textured material, transients soften |

### Dynamics

All work offline on a peak envelope and return the input length.

| Method | Notes |
| --- | --- |
| `compress(samples, *, threshold_db=-18, ratio=4, attack=0.005, release=0.08, knee_db=6, makeup_db=0)` | Soft-knee downward compressor; no look-ahead, so the first milliseconds of a hit pass through |
| `limit(samples, *, ceiling=0.98, lookahead=0.002, release=0.05)` | Brickwall: sample peaks never exceed `ceiling`; gain dips ahead of peaks and recovers over `release` |
| `gate(samples, *, threshold_db=-50, attack=0.002, release=0.05, floor_db=-80)` | Falls to `floor_db` while the signal is below the threshold |
| `shape_transients(samples, *, attack_db=6, sustain_db=0)` | Gain from a fast-vs-slow envelope: onsets get `attack_db`, the rest `sustain_db` |

### Timeline

| Method | Notes |
| --- | --- |
| `mix(layers, *, seconds=None)` | List or `{name: ...}` of arrays, `(at, samples)` / `(at, samples, gain)` tuples or `layer` values; length is the last layer end unless `seconds` pads/crops; negative starts are rejected |
| `layer(samples, *, at=0, gain=1)` | Read-only snapshot with placement and the context's seed metadata (for `review_clip` stems) |
| `pad(samples, seconds)`, `segment(samples, seconds, start=0)` | Append silence; slice with zero padding past the end |
| `loop(samples, crossfade)` | Seamless loop: the last `crossfade` seconds blend (equal power) into the start; the result is that much shorter and its end flows into its beginning |

### Level and export

| Method | Notes |
| --- | --- |
| `normalize(samples, *, peak=None, lufs=None)` | Exactly one target; silence stays silent |
| `loudness(samples)` | Integrated loudness, mono ITU-R BS.1770 (K-weighting, 400 ms blocks, absolute -70 and relative -10 LU gates); clips shorter than 400 ms are measured whole; `None` for silence |
| `trim(samples, *, threshold_dbfs=-60, window=0.01, hold=0.05, fade=0.01, minimum=0)` | Cut after the last window whose RMS (or peak 12 dB lower) is above the threshold; earlier gaps and late reflections are kept |
| `finish(samples, *, peak=0.9, lufs=None, ceiling=0.98, dc=True, fade_in=0.0005, fade_out=0.01, trim=-60)` | Export chain below |
| `load(path)` | WAV (8/16/24/32-bit, float) via SciPy, other formats via soundfile; channel mean, resampled to the context rate; DC and level untouched |
| `save(path, samples, *, bits=16, dither=False, quality=0.6)` | By suffix: `.wav` PCM 16/24 bit, `.flac` 16/24 bit, `.ogg` Vorbis (`quality` 0-1, higher is better). Clipped to full scale and rounded; `dither` adds seeded TPDF dither (±1 LSB) to WAV, useful for quiet 16-bit tails. Creates folders, overwrites, rejects NaN/Inf, returns the path; FLAC/OGG need soundfile |
| `pack(output, *, prefix="", format="wav", bits=16, dither=False, **finish)` | Sound set, see above |

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
| `export_gallery(path, sounds, *, title="Sound gallery", current=None)` | One self-contained HTML page for `{name: samples}`: player, waveform, spectrogram, peak/RMS/LUFS and warnings; sounds play at their true relative level; "Play all". `current` (`{name: samples}`) adds a New/Current switch that keeps the playback position, plus duration/peak/LUFS differences |
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
        oscillators.py     band-limited waves, unison, FM, noise colors, modal, thump, pluck
        filters.py         Butterworth and per-sample state-variable filters, EQ, formants, vowels
        effects.py         waveshaping/oversampling, bitcrush, delay, chorus, reverb, speed, follower
        dynamics.py        compressor, limiter, gate, transient shaper
        stretch.py         WSOLA time stretch and pitch shift
        timeline.py        layers, mix, pad, segment, loop, scatter
        level.py           normalize, BS.1770 loudness, trim, finish
        wave_io.py         WAV/FLAC/OGG writing, PCM encoding, dither, source loading
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

Both suites are registered with CTest under the label `audio-generation` (one test per Python suite, plus
the player suite when Node is installed). The Python suites are registered only when the configuring Python has
NumPy and SciPy. From a game: `python Engine test --label audio-generation`. Directly, from the Engine root:

```sh
python -B -m unittest discover -s AudioGeneration/Tests
node --test AudioGeneration/Tests/review_player.test.cjs
```

The Python tests use temporary files and only the public API (NumPy and SciPy required). The Node tests check
the review player's transport, gain, solo and saved selections in an in-memory DOM; they do not replace a
real-browser listen.
