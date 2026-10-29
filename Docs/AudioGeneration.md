# Offline audio generation

`AudioGeneration/` is an opt-in Python authoring library. It writes audio assets
offline and is independent of the C++ audio runtime, game catalogs and builds.
The only supported import is:

```python
from AudioGeneration import SoundGenerator
```

Add the Engine root to the Python import path in the project's authoring script
or launcher. For example, for a script at `MyProject/Audio/MakeSounds.py`:

```python
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Engine"))
from AudioGeneration import SoundGenerator

g = SoundGenerator(sample_rate=44100, seed=42)
samples = g.lowpass(g.noise(seconds=0.2), cutoff=1800)
samples = g.normalize(samples, peak=0.5)
g.fade(samples, attack=0.002, release=0.04)
g.write_wav(Path(__file__).parent / "Output" / "Rustle.wav", samples)
```

Paths, sound names, recipes, artistic choices and timing belong to the caller.
The library never discovers a game's assets, updates a catalog, builds a game,
downloads recordings or installs dependencies.

## Dependencies

| Operations | Requirements |
| --- | --- |
| Import, `sample_count`, `run`, `saw_sample`, `lowpass_samples`, rounded WAV export | Python standard library |
| Buffers, envelopes, harmonic sums, white noise, one-pole array filters, normalizing, saturation, ordinary WAV export | NumPy |
| Signal analysis, named layers, mixing and stem export | NumPy |
| Additive bandlimited oscillators, `smooth_filter`, `trim_tail_energy` | NumPy |
| `saturate(..., oversample=2/4/8/16)` | NumPy + SciPy |
| Named seed contexts, report formatting and text/JSON report export | Python standard library |
| Butterworth and swept filters, brown noise, textures, reverb, SciPy waveforms | NumPy + SciPy |
| `read_source` | NumPy + SciPy + soundfile |

Install the dependencies needed by the recipes in your authoring environment:
`python -m pip install numpy scipy`; add `soundfile` when decoding source files.
Dependencies load only when their operations are used. Production game binaries
do not use this Python library.

## Structure and extension

```text
AudioGeneration/
    __init__.py                 supported import: SoundGenerator
    generator.py                public API and sample-rate/RNG context
    contracts.py                returned data values (constructed via the facade)
    Internals/
        buffers.py              time, phase, layering, segments, normalization
        envelopes.py            amplitude shapes, fades, tail trimming
        filters.py              fixed and time-varying filters, formants
        oscillators.py          harmonic sums, noise helpers, pulse
        bandlimited.py          guarded additive saw and square oscillators
        smooth_filters.py       sample-wise trapezoidal state-variable filter
        tails.py                RMS/peak-window trailing-energy detector
        validation.py           shared quality-path input contracts
        textures.py             seeded crackles and pings
        effects.py              convolution room, saturation
        scalar.py               standard-library synthesis/filter primitives
        wave_io.py              PCM16 export and optional source decoding
        cli.py                  named-sound selection and progress messages
        seeding.py              stable named sound/layer seed derivation
        analysis.py             raw signal and per-frame spectral measurements
        reports.py              text and strict JSON serialization
        layers.py               named snapshots, aligned mixes and stem export
    Tests/
        test_sound_generator.py existing synthesis/export behavior
        test_authoring_workflow.py analysis, named layers and independent seeds
        test_signal_quality.py  spectral rejection, filtering and tail behavior
```

Add supported operations to `SoundGenerator` and keep implementation in the
matching internal file. Consumers must not import `Internals`, `generator.py`
or third-party backends to access library operations. New categories can have
their own internal file without changing the public import. Keep all game
content outside the library; do not create game-specific presets here.

Existing synthesis algorithms retain their distinct rounding, filter and gain
conventions. Analysis, named layers, independent seed contexts and quality
processing are opt-in; they do not alter existing recipes. Automatic mastering
and a recipe serialization format are not provided.

## Buffer and ownership contract

Most signal operations accept and return one-dimensional floating NumPy arrays
representing mono samples. `lowpass_samples` and `saw_sample` use scalar Python
values/lists. Seconds and Hz are used unless a parameter explicitly says count.
Gain and peak values are linear amplitudes, not dB.

`SoundGenerator(sample_rate=44100, rounding="round", seed=None)` owns a lazy RNG.
`rounding="truncate"` preserves recipes that previously used `int(seconds * rate)`.
`rng` exposes the same NumPy generator used by noise, textures and reverb so
recipes can preserve their existing shared draw order. Independent per-sound
seeding is applied only when requested through `for_sound`/`for_layer`.
Reproducibility requires the same seed, calls within each context and dependency
versions; do not share a context between concurrent renders.

Methods generally return new arrays. `mix_at` modifies and returns its target;
`fade` modifies and returns its input unless `copy=True`. `trim_tail` and `segment`
return new arrays. No method silently normalizes another operation's result.

## Supported operations

| Methods | Contract |
| --- | --- |
| `sample_count(seconds)`, `time_axis(seconds)` | Context rounding; time starts at zero and excludes the end |
| `phase_of(frequency)` | Integrate an array of Hz values to radians |
| `sweep_phase(t, start, end, seconds)` | Existing exponential frequency contour; last argument is its time constant |
| `mix_at(target, sound, at, gain=1)` | Add in place at a nonnegative time, crop to target length; ignore layers starting beyond the target |
| `pad(samples, seconds)` | Append silence |
| `segment(samples, seconds, start=0)` | Slice at a nonnegative offset, zero-pad if the source ends |
| `normalize(samples, peak, epsilon=1e-9, additive=False, divide_first=False)` | Scale to peak using a floor denominator; `additive` selects max amplitude + epsilon; `divide_first` preserves division-before-gain arithmetic |
| `linear_envelope(t, attack, release)` | Linear entrance and exit |
| `decay_envelope(t, attack, decay)` | Exponential entrance and decay |
| `attack_release(count, attack, release, curve=1)` | Non-overlapping entrance/exit using truncated sample counts |
| `fade(samples, attack=0, release=0, curve=1, copy=False)` | Independent edge ramps using context rounding; overlapping ramps multiply |
| `trim_tail(samples, floor=0.002)` | Trim after the last above-floor sample, retaining a 10 ms fade |
| `trim_tail_energy(samples, threshold_dbfs=-60, window_seconds=0.01, hold_seconds=0.05, fade_seconds=0.01, min_seconds=0, peak_guard_db=12)` | Trim after the last energetic window, retaining late reflections, hold and fade time |
| `lowpass(samples, cutoff, order=2)`, `highpass(...)` | Butterworth SOS filtering |
| `bandpass(samples, low, high, order=2)` | Butterworth SOS band-pass |
| `one_pole_lowpass(samples, cutoff)` | Array one-pole filter; cutoff can vary per sample |
| `one_pole_highpass(samples, cutoff)`, `one_pole_bandpass(samples, low, high)` | Existing one-pole subtraction/cascade algorithms |
| `lowpass_samples(samples, cutoff)` | Scalar/list one-pole implementation |
| `sweep_filter(samples, kind, cutoff, order=2, width=None, block_size=256)` | Block-updated SOS with carried state; `kind="band"` uses a relative width |
| `swept_filter(samples, cutoff_at, kind="lowpass", order=2, blocks=64)` | Overlapping Hann-windowed filters; callback takes seconds and returns cutoff Hz or a pair |
| `sweep_bandpass(samples, start, end, width, blocks=64)` | Exponential band-center sweep using the overlapping-window algorithm |
| `smooth_filter(samples, cutoff, kind="lowpass", q=2**-0.5, smoothing_seconds=0.005)` | Sample-wise TPT state-variable filter with log-frequency smoothing |
| `formant_voice(source, formants)` | Sum bands described by `(center curve, width ratio, gain)` |
| `noise(seconds, count=None)` | Uniform noise in [-1, 1], using the context RNG |
| `brown(seconds)` | Existing leaky-integrated Gaussian noise |
| `smooth_random(seconds, rate, low=0, high=1)` | Linearly interpolated random control curve |
| `saw(frequency, t, harmonics=24)` | Fixed-frequency harmonic sum with a Nyquist cutoff |
| `bandlimited_saw(frequency, seconds=None, phase=0, max_harmonics=512)` | Rising additive saw, limiting and tapering upper partials |
| `bandlimited_square(frequency, seconds=None, phase=0, max_harmonics=512)` | Additive square using only odd-numbered partials |
| `saw_phase(phase, harmonics=40)` | Fixed harmonic sum from phase, without a Nyquist cutoff |
| `saw_sample(phase, harmonics)` | Unscaled scalar harmonic sum |
| `pulse(t, frequency, width=0.3)` | Existing soft pulse waveform |
| `sawtooth(phase)`, `square(phase)` | Existing non-band-limited SciPy waveforms |
| `crackles(seconds, per_second, decay, color_low, color_high, gain_curve=None)` | Filtered random short bursts; retains truncated sample sizing |
| `pings(seconds, per_second, low, high, decay, gain_curve=None)` | Random resonant pings; retains truncated sample sizing |
| `reverb(samples, seconds=1.6, mix=0.25, damp=3500, predelay=0.012)` | Noise-impulse convolution room with its complete tail |
| `saturate(samples, drive, oversample=1)` | Existing normalized tanh; optional filtered 2x/4x/8x/16x processing |
| `read_source(path)` | soundfile decode, channel mean, polyphase resampling to context rate, DC removal |
| `write_wav(path, samples, rounding=False, clip=True)` | Mono PCM16; creates parent folders and overwrites the named file |
| `run(sounds, write, prefix="", names=None, validate=True)` | Select names, call `write(name)` and print filenames; no automatic recipe invocation |
| `for_sound(name, variation=0)`, `for_layer(name, variation=0)` | Fresh independently seeded child context; names are nonempty strings, variation is a nonnegative integer |
| `seed_info` | Fresh dictionary with effective integer `seed` and named `scopes`; an unrecorded/noninteger root seed is reported as null |
| `analyze(samples, silence_dbfs=-60, frame_seconds=0.02, edge_dbfs=-40)` | Raw signal and per-frame analysis as a JSON-safe dictionary |
| `format_report(report)` | Human-readable audio or layer report; does not write files |
| `write_report(path, report)` | Write `.json` and `.txt` siblings; return their paths |
| `layer(samples, at=0, gain=1)` | Owned read-only sample snapshot, placement, gain and seed metadata |
| `mix_layers(layers, seconds=None)` | Mix a name-to-array/layer mapping into a new mono array |
| `analyze_layers(layers, seconds=None, silence_dbfs=-60, frame_seconds=0.02, edge_dbfs=-40)` | Analyze mix and aligned layer contributions, with placement and crop metadata |
| `export_layers(directory, layers, seconds=None, report=True)` | Export the mix, aligned stems and optional text/JSON measurements; return file paths |

Optional arguments after `*` in the Python signatures must be passed by keyword.
`write_wav` normally clips and truncates NumPy values. `rounding=True` uses the
standard-library rounded/clipped scalar quantizer; `clip` applies only to the
NumPy path. Source-specific silence/finite checks and gain caps remain in recipes.
Bad paths, missing dependencies and invalid filter settings propagate the backend
exception. `run` validates all names before writing, raising `SystemExit` with the
choices; `validate=False` retains direct mapping-lookup behavior. By default it
uses command-line names or, with no names, mapping insertion order. Supplying
`names` overrides command-line selection.

## Named seeds and layers

```python
import numpy as np

root = SoundGenerator(sample_rate=44100, seed=42)
sound = root.for_sound("Impact", variation=0)
body = sound.for_layer("body")
air = sound.for_layer("air")

t = body.time_axis(0.5)
body_samples = np.sin(2 * np.pi * 70 * t) * np.exp(-t / 0.08)
air_samples = air.bandpass(air.noise(0.25), 800, 5000)
layers = {
    "body": body.layer(body_samples, gain=0.6),
    "air": air.layer(air_samples, at=0.01, gain=0.2),
}

samples = sound.mix_layers(layers)
report = sound.analyze_layers(layers)
print(sound.format_report(report))
paths = sound.export_layers(Path("Output") / "Impact", layers)
```

`for_sound` and `for_layer` derive their effective seeds from the parent seed,
scope kind, exact case-sensitive UTF-8 name and variation using SHA-256. They do
not consume the parent RNG or use Python's randomized `hash()`. Adding a sibling,
changing generation order or generating one sound alone does not change another
named context's random samples. Changing a scope name, ancestor or variation does.
Store a context once while building a recipe: requesting the same context again
returns a fresh stream starting at the same seed, rather than continuing it.

Named scopes require an integer root seed or `None`. With `None`, scope entropy
is chosen once per parent instance, so repeated lookups still match within that
instance; new parent instances differ. The effective seed in `seed_info` can be
passed to a new `SoundGenerator` to reproduce that context and its descendants.
Existing direct `rng` use continues to accept NumPy's seed forms and retain its
original call-order behavior. A seed alone does not record earlier draws; save
the recipe, variation and dependency versions too.

`layer()` captures samples and the originating context's seed metadata before
later processing can mutate the input array. Returned `AudioLayer` values have
`samples`, `sample_rate`, `at_seconds`, `gain` and `random` data fields; construct
them through the facade. Raw arrays are accepted in the mapping as shorthand for
a layer starting at zero with gain one and the mix context's seed metadata.
Use explicitly scoped layer values when provenance matters.

The default timeline ends at the last layer's end. `seconds` explicitly pads or
crops it. Start times must be nonnegative, gains finite, and sample rates must
match. Gains may be negative for polarity inversion. Mixes are not normalized,
limited, faded or trimmed. Per-layer reports record original and retained sample
counts so cropping is visible. An empty mapping is rejected; a named empty signal
can still be inspected.

`export_layers` writes `mix.wav`, `layers/001-<name>.wav`, etc., plus `analysis.json`
and `analysis.txt` by default. Filenames are sanitized and numbered to avoid name
collisions; returned path mappings retain the original names. Every stem has the
same duration as the mix, including its placement silence and applied gain.
Stems retain their relative levels. Quantization/clipping means exported integer
stems need not sum exactly to the exported integer mix. Reports measure the raw
floating signals before PCM conversion, making potential export clipping visible.
NaN/Inf can be analyzed, but layered export rejects them before creating files.
Existing `write_wav` behavior is unchanged. Output files are overwritten.

## Analysis and reports

`analyze` never modifies the input or adjusts its gain. The result contains:

- Format, duration, peak/RMS amplitudes and dBFS, crest factor and DC offset.
- Separate NaN/positive-Inf/negative-Inf counts and samples exceeding full scale.
- Active time, first-to-last active span, leading/trailing silence using the
  absolute `silence_dbfs` threshold. Active time counts only above-threshold
  samples; the span includes gaps. An entirely silent signal has its full duration
  as both leading and trailing silence; these are not disjoint intervals.
- Start/end amplitudes and largest adjacent sample step. Edge warnings describe
  possible boundary clicks, not a verified audible defect.
- Hann-windowed spectral centroid (magnitude-weighted), power spectral flatness
  and one-sided energy fractions: sub [0, 80), bass [80, 250), mid [250, 4000),
  treble [4000, Nyquist] Hz. Low sample rates naturally leave upper bands empty.
- Per-frame start, duration, peak, RMS and the same spectral measurements,
  including the shorter final frame. Default frames are 20 ms; frequency
  resolution depends on frame duration. Whole-signal metrics use the whole signal,
  not an average of per-frame dB values.
- Structured warnings (`code`, `message`) and the context's seed information.

Silence has amplitude zero and undefined dBFS/crest/spectral values are JSON null.
Empty signals retain format metadata but have no derived measurements. NaN/Inf
signals expose invalid/clipping counts and warnings; derived metrics are null
and frames omitted until those invalid values are fixed. Numeric overflow in an
otherwise valid measurement also yields null. Reports use strict JSON without
NaN or Infinity literals, and carry `schema_version: 1` and a `kind` of `audio` or
`layers`. Text summaries are for scanning; JSON retains full numeric precision
and per-frame data.

`format_report` returns the summary without I/O. `write_report` replaces the
supplied path's final suffix with `.json` and `.txt` and returns
`{"json": Path(...), "text": Path(...)}`. `export_layers` additionally returns
`{"mix": Path(...), "layers": {name: Path(...)}, "report": {...}}`; the `report`
entry is omitted when `report=False`.

These measurements describe signal structure, not artistic quality, perceived
loudness or true peak. No LUFS estimator, reference comparison, automatic
mastering, waveform plot or spectrogram image is included.

## Opt-in signal quality

```python
g = SoundGenerator(sample_rate=48000, seed=42).for_sound("Growl")
t = g.time_axis(0.8)
pitch = 100 + 70 * np.exp(-t / 0.15)
voice = g.bandlimited_saw(pitch)
voice *= g.decay_envelope(t, 0.005, 0.25)
voice = g.saturate(voice, drive=3.0, oversample=4)
cutoff = 300 + 2500 * np.exp(-t / 0.3)
voice = g.smooth_filter(voice, cutoff, smoothing_seconds=0.005)
voice = g.reverb(voice, seconds=1.2, mix=0.2)
voice = g.trim_tail_energy(voice, threshold_dbfs=-65)
print(g.format_report(g.analyze(voice)))
g.write_wav(Path("Output") / "Growl.wav", voice)
```

These operations do not normalize or clip their floating output. Inspect the
result before choosing export gain. New quality paths require finite real mono
signals and valid finite controls. They return new arrays without mutating their
inputs. Existing `saw`, `saw_phase`, `sawtooth`, `square`, `sweep_filter`,
`swept_filter`, `trim_tail` and default `saturate` behavior remain unchanged.

### Additive oscillators

Supply scalar Hz plus `seconds`, or a one-dimensional Hz curve with one value
per output sample. A curve determines its own length; if `seconds` is also given,
its rounded/truncated sample count must match. Phase starts at the supplied
`phase` in radians (wrapped modulo 2 pi) and advances using the previous sample's frequency. Frequency
must stay strictly between zero and Nyquist. Empty curves return empty arrays.

The saw is a rising Fourier saw; square uses positive sine terms at odd harmonics.
Neither introduces a DC term. Higher partials taper with a cosine between 80%
and 95% of Nyquist and are omitted above that range. The fundamental is retained
below Nyquist even when it lies inside the guard region. This avoids hard
partial switches during pitch sweeps and leaves a high-frequency guard band.
`max_harmonics` is the largest harmonic number allowed, not the number of partials
in a square wave; its default 512 bounds offline synthesis cost. Low notes can
be made brighter at greater cost by increasing it.

For constant pitch, only legal harmonics are synthesized, preventing the folded
partials of a naive discontinuous waveform. Rapid FM or amplitude modulation can
still generate sidebands; an instantaneous harmonic limit is not a guarantee of
zero aliasing for arbitrary modulation. Do not repair an already aliased signal
by merely replacing a later filter. Fourier truncation may overshoot amplitude
one, and waveform polarity/phase differ from the legacy `saw` helper; these are
explicit new oscillators rather than silent replacements.

### Oversampled saturation

`oversample=1` runs the original NumPy-only calculation exactly. Choose 2, 4, 8
or 16 to use symmetric Kaiser-windowed FIR polyphase upsampling, apply normalized
tanh at the elevated rate, and filter before downsampling. The FIR uses 64
elevated-rate samples per factor on each side, Kaiser beta 8.6, and cutoff at
94% of the output Nyquist frequency. This reserves a transition band and can
attenuate content very close to Nyquist. Higher factors cost more CPU and reduce,
but do not mathematically eliminate, nonlinear aliasing.

Endpoint values are extended before filtering, and guards and filter delay are
removed afterward. Returned sample count and timeline alignment match the input;
no reverb tail is added. Filtering can create pre/post ringing around sharp
transients. Empty and one-sample signals are supported; on this quality path,
zero drive bypasses the operation and returns a copy. Drive must be nonnegative.
No limiter, level matching or oversampling-dependent peak normalization is applied.

Polyphase processing and custom symmetric FIR alignment follow the
[SciPy resample_poly contract](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.resample_poly.html).

### Smooth filter movement

`smooth_filter` is a separate two-pole trapezoidal state-variable filter, not a
smoothed wrapper around the existing Butterworth implementation. It updates each
sample while retaining integrator state. Scalar cutoff or a matching per-sample
Hz curve is accepted, with cutoff strictly between zero and `0.49 * sample_rate`.
Choose `lowpass`, `highpass` or `bandpass`; Q is constant, positive and defaults
to a Butterworth-like `1/sqrt(2)`. Band-pass gain at its center depends on Q.

An exponential smoother operates on log Hz, producing frequency-ratio transitions
with the given time constant in seconds. The first cutoff initializes the control
state; `smoothing_seconds=0` follows the supplied curve immediately. Each call
starts with zero filter integrator state, so process a complete offline clip in
one call. It is not a streaming/chunk-state API. Fast control changes or high Q
can still cause strong resonant output; no automatic gain correction is applied.

The mathematical topology is derived from Andrew Simper's
[trapezoidal state-variable filter paper](https://cytomic.com/files/dsp/SvfLinearTrapOptimised2.pdf),
with sample-wise log-frequency smoothing added here.

### Energy-aware tail trimming

`trim_tail_energy` examines every non-overlapping RMS window across the complete
clip. A window remains active when RMS exceeds `threshold_dbfs`, or its peak
exceeds `threshold_dbfs + peak_guard_db`. The peak guard protects sparse short
reflections that a longer RMS average could hide. It does not stop at the first
quiet window, so gaps before later reflections remain intact.

After the last active window, it retains `hold_seconds` plus `fade_seconds`,
then applies a linear fade only to the final fade region. Duration never exceeds
the supplied clip; if the original clip ends while still loud, the final fade
necessarily affects that ending. All durations use rounded sample counts.
The threshold is absolute dBFS, not relative to each clip's peak. Tune it for very
quiet assets; nothing above the thresholds means an empty result unless
`min_seconds` requests a minimum retained duration. Empty input stays empty.
Original placement, leading silence and any internal gaps are preserved.

## Tests

From the Engine repository root:

```sh
python -B -m unittest discover -s AudioGeneration/Tests
```

These tests use temporary files, exercise only the public API and do not build
or launch anything. They require NumPy and SciPy; source decoder tests can supply
fixtures without requiring an external recording.
