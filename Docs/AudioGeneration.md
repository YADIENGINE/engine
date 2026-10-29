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
    Internals/
        buffers.py              time, phase, layering, segments, normalization
        envelopes.py            amplitude shapes, fades, tail trimming
        filters.py              fixed and time-varying filters, formants
        oscillators.py          harmonic sums, noise helpers, pulse
        textures.py             seeded crackles and pings
        effects.py              convolution room, saturation
        scalar.py               standard-library synthesis/filter primitives
        wave_io.py              PCM16 export and optional source decoding
        cli.py                  named-sound selection and progress messages
    Tests/
        test_sound_generator.py public behavior tests
```

Add supported operations to `SoundGenerator` and keep implementation in the
matching internal file. Consumers must not import `Internals`, `generator.py`
or third-party backends to access library operations. New categories can have
their own internal file without changing the public import. Keep all game
content outside the library; do not create game-specific presets here.

This first extraction retains the existing algorithms, including their distinct
rounding, filter and gain conventions. It does not add analysis, automatic
mastering, improved oscillators or a recipe serialization format.

## Buffer and ownership contract

Most signal operations accept and return one-dimensional floating NumPy arrays
representing mono samples. `lowpass_samples` and `saw_sample` use scalar Python
values/lists. Seconds and Hz are used unless a parameter explicitly says count.
Gain and peak values are linear amplitudes, not dB.

`SoundGenerator(sample_rate=44100, rounding="round", seed=None)` owns a lazy RNG.
`rounding="truncate"` preserves recipes that previously used `int(seconds * rate)`.
`rng` exposes the same NumPy generator used by noise, textures and reverb so
recipes can preserve their existing shared draw order. No independent per-sound
seeding is applied. Reproducibility requires the same seed, calls and dependency
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
| `lowpass(samples, cutoff, order=2)`, `highpass(...)` | Butterworth SOS filtering |
| `bandpass(samples, low, high, order=2)` | Butterworth SOS band-pass |
| `one_pole_lowpass(samples, cutoff)` | Array one-pole filter; cutoff can vary per sample |
| `one_pole_highpass(samples, cutoff)`, `one_pole_bandpass(samples, low, high)` | Existing one-pole subtraction/cascade algorithms |
| `lowpass_samples(samples, cutoff)` | Scalar/list one-pole implementation |
| `sweep_filter(samples, kind, cutoff, order=2, width=None, block_size=256)` | Block-updated SOS with carried state; `kind="band"` uses a relative width |
| `swept_filter(samples, cutoff_at, kind="lowpass", order=2, blocks=64)` | Overlapping Hann-windowed filters; callback takes seconds and returns cutoff Hz or a pair |
| `sweep_bandpass(samples, start, end, width, blocks=64)` | Exponential band-center sweep using the overlapping-window algorithm |
| `formant_voice(source, formants)` | Sum bands described by `(center curve, width ratio, gain)` |
| `noise(seconds, count=None)` | Uniform noise in [-1, 1], using the context RNG |
| `brown(seconds)` | Existing leaky-integrated Gaussian noise |
| `smooth_random(seconds, rate, low=0, high=1)` | Linearly interpolated random control curve |
| `saw(frequency, t, harmonics=24)` | Fixed-frequency harmonic sum with a Nyquist cutoff |
| `saw_phase(phase, harmonics=40)` | Fixed harmonic sum from phase, without a Nyquist cutoff |
| `saw_sample(phase, harmonics)` | Unscaled scalar harmonic sum |
| `pulse(t, frequency, width=0.3)` | Existing soft pulse waveform |
| `sawtooth(phase)`, `square(phase)` | Existing non-band-limited SciPy waveforms |
| `crackles(seconds, per_second, decay, color_low, color_high, gain_curve=None)` | Filtered random short bursts; retains truncated sample sizing |
| `pings(seconds, per_second, low, high, decay, gain_curve=None)` | Random resonant pings; retains truncated sample sizing |
| `reverb(samples, seconds=1.6, mix=0.25, damp=3500, predelay=0.012)` | Noise-impulse convolution room with its complete tail |
| `saturate(samples, drive)` | Existing normalized tanh saturation |
| `read_source(path)` | soundfile decode, channel mean, polyphase resampling to context rate, DC removal |
| `write_wav(path, samples, rounding=False, clip=True)` | Mono PCM16; creates parent folders and overwrites the named file |
| `run(sounds, write, prefix="", names=None, validate=True)` | Select names, call `write(name)` and print filenames; no automatic recipe invocation |

Optional arguments after `*` in the Python signatures must be passed by keyword.
`write_wav` normally clips and truncates NumPy values. `rounding=True` uses the
standard-library rounded/clipped scalar quantizer; `clip` applies only to the
NumPy path. Source-specific silence/finite checks and gain caps remain in recipes.
Bad paths, missing dependencies and invalid filter settings propagate the backend
exception. `run` validates all names before writing, raising `SystemExit` with the
choices; `validate=False` retains direct mapping-lookup behavior. By default it
uses command-line names or, with no names, mapping insertion order. Supplying
`names` overrides command-line selection.

## Tests

From the Engine repository root:

```sh
python -B -m unittest discover -s AudioGeneration/Tests
```

These tests use temporary files, exercise only the public API and do not build
or launch anything. They require NumPy and SciPy; source decoder tests can supply
fixtures without requiring an external recording.
