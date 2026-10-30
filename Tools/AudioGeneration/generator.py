"""The public facade for offline audio authoring; implementations live in Internals."""

from os import PathLike
import re

import numpy as np

from .Internals import (analysis, comparison, curves, dynamics, effects, filters, gallery, level, oscillators,
                        reports, review, review_io, seeding, stretch, timeline, validation, wave_io)
from .Internals.pack import SoundPack

NOTE_PATTERN = re.compile(r"^([A-Ga-g])([#b]?)(-?\d+)$")
NOTE_STEPS = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}


class SoundGenerator:
    """Sample-rate context with its own seeded NumPy RNG.

    Signals are one-dimensional float arrays (mono). Times are seconds, frequencies Hz, gains linear.
    Every operation validates its input and returns a new array; nothing is normalized implicitly.
    Parameters that accept a "curve" take either a scalar or one value per output sample.
    """

    def __init__(self, sample_rate=44100, *, seed=0):
        self.sample_rate = validation.positive_integer(sample_rate, "sample_rate")
        self._seed = seeding.root(seed)
        self._scope_path = ()
        self._rng = None

    # Seeds ------------------------------------------------------------------------------------------------------

    @property
    def seed(self):
        return self._seed

    @property
    def seed_info(self):
        """Effective integer seed plus the named scopes that produced it."""
        return seeding.describe(self._seed, self._scope_path)

    @property
    def rng(self):
        """This context's NumPy generator, for recipe-level random choices."""
        if self._rng is None:
            self._rng = np.random.default_rng(self._seed)
        return self._rng

    def _named(self, kind, name, variation):
        seed, identity = seeding.child(self._seed, kind, name, variation)
        child = SoundGenerator(self.sample_rate, seed=seed)
        child._scope_path = self._scope_path + (identity,)
        return child

    def for_sound(self, name, *, variation=0):
        """Fresh context seeded from (seed, name, variation); siblings and call order never affect it."""
        return self._named("sound", name, variation)

    def for_layer(self, name, *, variation=0):
        """Fresh child context for one layer of a sound."""
        return self._named("layer", name, variation)

    # Time -------------------------------------------------------------------------------------------------------

    def sample_count(self, seconds):
        return round(validation.nonnegative(seconds, "seconds") * self.sample_rate)

    def _count(self, seconds):
        return None if seconds is None else self.sample_count(seconds)

    def time_axis(self, seconds):
        return curves.time_axis(self.sample_count(seconds), self.sample_rate)

    def silence(self, seconds):
        return np.zeros(self.sample_count(seconds))

    # Control curves -----------------------------------------------------------------------------------------------

    def envelope(self, points, seconds=None, *, curve=1.0):
        """Breakpoints [(seconds, value), ...]; curve < 1 moves fast first, > 1 slow first."""
        end = seconds if seconds is not None else max(at for at, _ in points)
        return curves.envelope(points, self.sample_count(end), self.sample_rate, curve)

    def contour(self, points, seconds=None):
        """Breakpoints interpolated geometrically: natural for pitch, cutoff and other ratios."""
        end = seconds if seconds is not None else max(at for at, _ in points)
        return curves.contour(points, self.sample_count(end), self.sample_rate)

    def adsr(self, seconds, attack, decay, sustain, release, *, curve=1.0):
        """Attack/decay/sustain/release ending at `seconds`; stages shrink proportionally if they do not fit."""
        return curves.adsr(self.sample_count(seconds), self.sample_rate, attack, decay, sustain, release, curve)

    def decay(self, seconds, time_constant, *, attack=0.0, hold=0.0):
        """Linear rise over `attack`, full level for `hold`, then exp(-t / time_constant)."""
        return curves.decay(self.sample_count(seconds), self.sample_rate, time_constant, attack, hold)

    def rise(self, seconds, time, *, curve=1.0):
        """0 to 1 over `time`, then 1 until `seconds`; curve < 1 rises fast first, > 1 slow first."""
        return curves.rise(self.sample_count(seconds), self.sample_rate, time, curve)

    def glide(self, start, end, seconds, time_constant):
        """Exponential approach from start to end (pitch drops, cutoff falls)."""
        return curves.glide(self.sample_count(seconds), self.sample_rate, start, end, time_constant)

    def lfo(self, seconds, rate, *, shape="sine", phase=0.0, center=0.0, depth=1.0):
        """center + depth * wave, wave in [-1, 1]: sine, triangle, square or saw; rate may be a curve.

        Vibrato: pitch * g.lfo(s, 6, center=1, depth=0.02). Tremolo 0.5-1: g.lfo(s, 9, center=0.75, depth=0.25).
        """
        return curves.lfo(self.sample_count(seconds), self.sample_rate, rate, shape, phase, center, depth)

    def random_curve(self, seconds, rate, *, low=-1.0, high=1.0):
        """Smooth (cosine-interpolated) random values, `rate` new targets per second."""
        return curves.random_curve(self.rng, self.sample_count(seconds), self.sample_rate, rate, low, high)

    def fade(self, samples, fade_in=0.0, fade_out=0.0, *, curve=1.0):
        return curves.fade(samples, self.sample_count(fade_in), self.sample_count(fade_out), curve)

    @staticmethod
    def note(value):
        """Hz of a MIDI number or a name such as "A4", "C#5", "Eb3" (A4 = 440 Hz)."""
        if isinstance(value, str):
            match = NOTE_PATTERN.match(value.strip())
            if not match:
                raise ValueError(f"Not a note name: {value!r}")
            letter, accidental, octave = match.groups()
            value = 12 * (int(octave) + 1) + NOTE_STEPS[letter.lower()] + {"#": 1, "b": -1, "": 0}[accidental]
        return 440.0 * 2.0 ** ((validation.finite(value, "note") - 69) / 12)

    # Sources ----------------------------------------------------------------------------------------------------

    def phase(self, frequency, seconds=None, *, start=0.0):
        """Integrate a Hz scalar/curve to radians, beginning exactly at `start`."""
        return oscillators.phase(frequency, self._count(seconds), self.sample_rate, start)

    def sine(self, frequency, seconds=None, *, phase=0.0):
        return oscillators.sine(frequency, self._count(seconds), self.sample_rate, phase)

    def saw(self, frequency, seconds=None, *, phase=0.0, max_harmonics=512):
        """Band-limited rising saw (no aliasing for slow pitch movement)."""
        return oscillators.additive(frequency, self._count(seconds), self.sample_rate, phase, max_harmonics, "saw")

    def square(self, frequency, seconds=None, *, phase=0.0, max_harmonics=512):
        return oscillators.additive(frequency, self._count(seconds), self.sample_rate, phase, max_harmonics,
                                    "square")

    def triangle(self, frequency, seconds=None, *, phase=0.0, max_harmonics=512):
        return oscillators.additive(frequency, self._count(seconds), self.sample_rate, phase, max_harmonics,
                                    "triangle")

    def pulse(self, frequency, seconds=None, *, width=0.5, phase=0.0, max_harmonics=512):
        """Band-limited DC-free pulse; `width` is the high fraction of each cycle."""
        return oscillators.pulse(frequency, self._count(seconds), self.sample_rate, phase, width, max_harmonics)

    def unison(self, frequency, seconds=None, *, wave="saw", voices=7, detune=20.0, max_harmonics=512):
        """Detuned stack (supersaw): `voices` spread over +/- `detune` cents with random phases, RMS-balanced."""
        return oscillators.unison(self.rng, frequency, self._count(seconds), self.sample_rate, wave, voices, detune,
                                  max_harmonics)

    def fm(self, frequency, seconds=None, *, ratio=1.0, index=1.0, phase=0.0):
        """Two-operator FM: sin(carrier + index * sin(ratio * carrier)). Index/ratio may be curves."""
        return oscillators.fm(frequency, self._count(seconds), self.sample_rate, ratio, index, phase)

    def noise(self, seconds, *, color="white"):
        """White (uniform), pink (1/f) or brown (1/f^2) noise with peak 1."""
        return oscillators.noise(self.rng, self.sample_count(seconds), self.sample_rate, color)

    def modal(self, seconds, frequency, ratios, decay, *, gains=None, falloff=0.5, attack=0.0005,
              random_phase=True, spread=0.0, click=0.0):
        """Struck resonator bank (metal, glass, wood, bells): partial i rings at frequency * ratios[i].

        `decay` is seconds per partial or one value shortened by (1 + falloff * i); gains default to 1 / (i + 1).
        `spread` detunes each partial randomly by up to +/- that fraction (every strike differs); `click` adds
        the strike's contact noise at that gain.
        """
        return oscillators.modal(self.rng, self.sample_count(seconds), self.sample_rate, frequency, ratios, decay,
                                 gains, falloff, attack, random_phase, spread, click)

    def burst(self, seconds, decay, *, band=None, lowpass=None, highpass=None, attack=0.0, color="white"):
        """Noise shaped by at most one filter and a decay: hits, clicks, breath, rumble tails.

        `band` is (low, high) Hz (either may be a curve); `decay` is the time constant in seconds.
        """
        return oscillators.burst(self.rng, self.sample_count(seconds), self.sample_rate, lowpass, band, highpass,
                                 decay, attack, color)

    def crackle(self, seconds, grains, band, *, within=None, length=(0.004, 0.012), decay=0.003, gain=(0.3, 1.0),
                envelope=None):
        """`grains` short noise grains at random onsets, band-passed to `band` (fire, ice, gravel, sparks).

        `within` is (start, end) seconds or a zero-argument callable returning an onset; `length` and `gain`
        are (min, max) ranges per grain; `envelope` is a per-sample curve read at each onset.
        """
        return oscillators.crackle(self.rng, self.sample_count(seconds), self.sample_rate, grains, band, within,
                                   length, decay, gain, envelope)

    def thump(self, seconds, start, end, *, sweep=0.03, decay=0.15, click=0.0):
        """Pitch-dropping body hit (kick, heartbeat, impact) with an optional noise click."""
        return oscillators.thump(self.rng, self.sample_count(seconds), self.sample_rate, start, end, sweep, decay,
                                 click)

    def pluck(self, frequency, seconds, *, decay=1.0, brightness=0.6):
        """Karplus-Strong plucked string, exactly tuned (all-pass fraction); `decay` is the time to -60 dB."""
        return oscillators.pluck(self.rng, self.sample_count(seconds), self.sample_rate, frequency, decay,
                                 brightness)

    def scatter(self, seconds, density, grain, *, gain=None):
        """Place round(density * seconds) grains at random onsets; `grain` is an array or a zero-argument callable."""
        return timeline.scatter(self.rng, self.sample_count(seconds), self.sample_rate, density, grain, gain)

    # Filters ----------------------------------------------------------------------------------------------------

    def lowpass(self, samples, cutoff, *, order=2):
        """Butterworth for a fixed cutoff; a cutoff curve uses smoothed state-variable stages."""
        return filters.pass_filter(samples, cutoff, "lowpass", order, self.sample_rate)

    def highpass(self, samples, cutoff, *, order=2):
        return filters.pass_filter(samples, cutoff, "highpass", order, self.sample_rate)

    def bandpass(self, samples, low=None, high=None, *, center=None, width=None, order=2):
        """Band between `low` and `high`, or `center` / width .. `center` * width (width is a ratio above 1).

        Fixed edges: Butterworth band-pass. Curves: `order` unity-peak resonant stages tracking the band.
        """
        return filters.bandpass(samples, low, high, order, self.sample_rate, center, width)

    def resonate(self, samples, center, *, q=4.0):
        """Unity-peak resonant band (center and q may be curves)."""
        return filters.resonate(samples, center, q, self.sample_rate)

    def formants(self, samples, bands):
        """Sum of resonant bands [(center Hz or curve, q, gain), ...] (voices, growls, vowels)."""
        return filters.formants(samples, bands, self.sample_rate)

    def dc_block(self, samples, cutoff=20.0):
        return filters.dc_block(samples, cutoff, self.sample_rate)

    def bell(self, samples, frequency, gain_db, *, q=1.0):
        """Peaking EQ: boost or cut `gain_db` around `frequency`; higher q is narrower."""
        return filters.equalize(samples, "bell", frequency, gain_db, q, self.sample_rate)

    def low_shelf(self, samples, frequency, gain_db, *, q=0.707):
        """Boost or cut everything below `frequency` by `gain_db`."""
        return filters.equalize(samples, "low", frequency, gain_db, q, self.sample_rate)

    def high_shelf(self, samples, frequency, gain_db, *, q=0.707):
        """Boost or cut everything above `frequency` by `gain_db`."""
        return filters.equalize(samples, "high", frequency, gain_db, q, self.sample_rate)

    def vowel(self, samples, shape, *, scale=1.0):
        """Vowel formants ("a", "e", "i", "o", "u") or [(seconds, vowel), ...] gliding between them.

        `scale` multiplies the formant frequencies: below 1 sounds like a larger throat.
        """
        return filters.vowel(samples, shape, scale, self.sample_rate)

    # Effects ----------------------------------------------------------------------------------------------------

    def saturate(self, samples, drive, *, shape="tanh", bias=0.0, oversample=4):
        """Oversampled waveshaper: "tanh" (soft), "clip" (hard) or "fold" (sine wavefolder).

        tanh and clip map input 1 to 1; `bias` shifts the curve for asymmetric, even-harmonic distortion
        (silence stays silent); drive 0 is a bypass.
        """
        return effects.saturate(samples, drive, shape, bias, oversample)

    def bitcrush(self, samples, *, bits=None, rate=None):
        """Quantize to `bits` and/or sample-and-hold at `rate` Hz."""
        return effects.bitcrush(samples, bits, rate, self.sample_rate)

    def delay(self, samples, time, *, feedback=0.4, mix=0.35, damp=None):
        """Feedback echo every `time` seconds; `damp` low-passes each repeat. Output includes the tail."""
        return effects.delay(samples, time, feedback, mix, damp, self.sample_rate)

    def chorus(self, samples, *, rate=0.8, depth=0.003, delay=0.015, mix=0.5):
        """Two modulated delay voices; short delay and depth give flanging."""
        return effects.chorus(samples, rate, depth, delay, mix, self.sample_rate)

    def reverb(self, samples, seconds=1.6, *, mix=0.25, damp=6000.0, predelay=0.012, impulse=None):
        """Unit-energy convolution room; output includes the tail.

        Synthetic room: octave bands decay over seconds / (1 + (f / damp)^2), so highs die first.
        `impulse` (samples or a file path) convolves with a recorded response instead.
        """
        if isinstance(impulse, (str, PathLike)):
            impulse = self.load(impulse)
        return effects.reverb(self.rng, samples, seconds, mix, damp, predelay, impulse, self.sample_rate)

    def speed(self, samples, factor):
        """Resample like a tape: factor 2 doubles pitch and halves duration."""
        return effects.speed(samples, factor)

    def follow(self, samples, *, attack=0.005, release=0.08):
        """Envelope follower (for ducking, gating or driving a filter from another sound)."""
        return effects.follow(samples, attack, release, self.sample_rate)

    def stretch(self, samples, factor):
        """Change duration without changing pitch (WSOLA): factor 2 is twice as long."""
        return stretch.stretch(samples, factor, self.sample_rate)

    def pitch_shift(self, samples, semitones):
        """Change pitch without changing duration (stretch, then resample)."""
        return stretch.pitch_shift(samples, semitones, self.sample_rate)

    # Dynamics ---------------------------------------------------------------------------------------------------

    def compress(self, samples, *, threshold_db=-18.0, ratio=4.0, attack=0.005, release=0.08, knee_db=6.0,
                 makeup_db=0.0):
        """Soft-knee downward compressor on the peak envelope."""
        return dynamics.compress(samples, threshold_db, ratio, attack, release, knee_db, makeup_db,
                                 self.sample_rate)

    def limit(self, samples, *, ceiling=0.98, lookahead=0.002, release=0.05):
        """Look-ahead brickwall limiter: sample peaks never exceed `ceiling`."""
        return dynamics.limit(samples, ceiling, lookahead, release, self.sample_rate)

    def gate(self, samples, *, threshold_db=-50.0, attack=0.002, release=0.05, floor_db=-80.0):
        """Noise gate: below the threshold the signal falls to `floor_db`."""
        return dynamics.gate(samples, threshold_db, attack, release, floor_db, self.sample_rate)

    def shape_transients(self, samples, *, attack_db=6.0, sustain_db=0.0):
        """Transient shaper: emphasize (or soften) onsets and sustain independently."""
        return dynamics.shape_transients(samples, attack_db, sustain_db, self.sample_rate)

    # Timeline ---------------------------------------------------------------------------------------------------

    def pad(self, samples, seconds):
        return timeline.pad(samples, self.sample_count(seconds))

    def segment(self, samples, seconds, start=0.0):
        """Slice `seconds` from `start`, zero-padded when the source ends."""
        return timeline.segment(samples, self.sample_count(seconds), self.sample_count(start))

    def layer(self, samples, *, at=0.0, gain=1.0):
        """Read-only snapshot with placement, gain and this context's seed metadata."""
        return timeline.layer(samples, self.sample_rate, at, gain, self.seed_info)

    def loop(self, samples, crossfade):
        """Seamless loop for drones and ambiences: the end cross-fades into the start (result is shorter)."""
        return timeline.loop(samples, self.sample_count(crossfade))

    def mix(self, layers, *, seconds=None):
        """Sum layers: a list or name mapping of arrays, (at, samples[, gain]) tuples or layer values.

        Length is the last layer end unless `seconds` pads or crops it.
        """
        return timeline.render(self, layers, seconds)[0]

    # Level ------------------------------------------------------------------------------------------------------

    def normalize(self, samples, *, peak=None, lufs=None):
        """Scale to a sample peak or an integrated loudness (exactly one); silence stays silent."""
        return level.normalize(samples, peak, lufs, self.sample_rate)

    def loudness(self, samples):
        """Integrated loudness in LUFS (mono BS.1770, gated); None for silence."""
        return level.loudness(samples, self.sample_rate)

    def trim(self, samples, *, threshold_dbfs=-60.0, window=0.01, hold=0.05, fade=0.01, minimum=0.0):
        """Cut after the last energetic window, keeping `hold` and a final `fade`; gaps are preserved."""
        return level.trim(samples, self.sample_rate, threshold_dbfs, window, hold, fade, minimum)

    def finish(self, samples, *, peak=0.9, lufs=None, ceiling=0.98, dc=True, fade_in=0.0005, fade_out=0.01,
               trim=-60.0):
        """Export chain: DC block -> peak or LUFS -> tail trim -> edge fades -> peak ceiling.

        Pass `peak=None` with `lufs`; `trim=None` keeps the full length; `dc` may be a cutoff in Hz.
        """
        if lufs is not None and peak is not None:
            raise ValueError("finish takes peak or lufs, not both")
        return level.finish(samples, self.sample_rate, peak, lufs, ceiling, dc, fade_in, fade_out, trim)

    # Files and sets ---------------------------------------------------------------------------------------------

    def load(self, path):
        """Decode a WAV (or, with soundfile installed, other formats) as mono at this context's rate."""
        return wave_io.load(path, self.sample_rate)[0]

    def save(self, path, samples, *, bits=16, dither=False, quality=0.6):
        """Write mono audio by suffix: .wav (PCM 16/24 bit), .flac (16/24 bit) or .ogg (Vorbis, quality 0-1).

        Samples are clipped to full scale; `dither` adds TPDF dither to WAV (seeded by this context).
        Creates folders and overwrites the file; .flac/.ogg need soundfile.
        """
        return wave_io.save(path, samples, self.sample_rate, bits, self.rng if dither else None, quality)

    def pack(self, output, *, prefix="", format="wav", bits=16, dither=False, **finish):
        """A named sound set writing <output>/<prefix><name>.<format>; other options are `finish` defaults."""
        return SoundPack(self, output, prefix, format, bits, dither, finish)

    # Inspection -------------------------------------------------------------------------------------------------

    def analyze(self, samples, *, silence_dbfs=-60.0, frame_seconds=0.02, edge_dbfs=-40.0):
        """Measure raw mono audio without changing it; return a JSON-safe report."""
        report = analysis.analyze(samples, self.sample_rate, silence_dbfs, frame_seconds, edge_dbfs)
        report["random"] = self.seed_info
        return report

    @staticmethod
    def format_report(report):
        """Readable summary of an audio, comparison or review report."""
        return reports.format_report(report)

    @staticmethod
    def write_report(path, report):
        """Write .json and .txt siblings and return their paths."""
        return reports.write_report(path, report)

    def export_gallery(self, path, sounds, *, title="Sound gallery", current=None):
        """Self-contained HTML listening page for {name: samples}, at true relative levels.

        `current` ({name: samples}, e.g. the files on disk) adds an A/B switch and level differences.
        """
        return gallery.export(self, path, gallery.entries_of(sounds, current), title)

    def review_clip(self, source=None, *, layers=None, seconds=None, parameters=None):
        """Capture a signal/file or layered recipe, with seed and JSON parameters."""
        return review_io.clip(self, source, layers, seconds, parameters)

    def compare(self, reference, candidate, *, focus="all", align=True, silence_dbfs=-60.0,
                onset_relative_db=-40.0, frame_seconds=0.01):
        """Compare active envelopes/stages without altering, stretching or mastering signals."""
        return comparison.compare(self, reference, candidate, focus, align, silence_dbfs,
                                  onset_relative_db, frame_seconds)

    def export_review(self, directory, reference, variants, *, title="Sound reference review", focus="all",
                      align=True, match="rms", target_dbfs=-20.0, max_gain_db=12.0,
                      silence_dbfs=-60.0, onset_relative_db=-40.0, frame_seconds=0.01):
        """Export an offline HTML A/B player against a reference, with plots, reports and stems."""
        return review.export(self, directory, reference, variants, title, focus, align, match,
                             target_dbfs, max_gain_db, silence_dbfs, onset_relative_db, frame_seconds)

