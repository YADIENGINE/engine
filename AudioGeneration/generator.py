"""The public facade for offline audio authoring; implementations live in Internals."""


class SoundGenerator:
    """Sample-rate context with an optional shared, lazily created NumPy RNG.

    Algorithms are explicit: Butterworth and one-pole filters, linear and
    exponential envelopes, and rounded and truncated sample counts stay distinct.
    Importing this class does not load optional authoring dependencies.
    """

    def __init__(self, sample_rate=44100, *, rounding="round", seed=None):
        if rounding not in ("round", "truncate"):
            raise ValueError("rounding must be 'round' or 'truncate'")
        self.sample_rate = sample_rate
        self.rounding = rounding
        self._seed = seed
        self._rng = None

    @property
    def rng(self):
        if self._rng is None:
            import numpy as np

            self._rng = np.random.default_rng(self._seed)
        return self._rng

    def sample_count(self, seconds):
        value = seconds * self.sample_rate
        return round(value) if self.rounding == "round" else int(value)

    def time_axis(self, seconds):
        from .Internals import buffers

        return buffers.time_axis(self.sample_count(seconds), self.sample_rate)

    def phase_of(self, frequency):
        from .Internals import buffers

        return buffers.phase_of(frequency, self.sample_rate)

    def sweep_phase(self, t, start, end, seconds):
        from .Internals import buffers

        return buffers.sweep_phase(t, start, end, seconds, self.sample_rate)

    def mix_at(self, target, sound, at, gain=1.0):
        from .Internals import buffers

        return buffers.mix_at(target, sound, self.sample_count(at), gain)

    def pad(self, samples, seconds):
        from .Internals import buffers

        return buffers.pad(samples, self.sample_count(seconds))

    def segment(self, samples, seconds, start=0.0):
        from .Internals import buffers

        return buffers.segment(samples, self.sample_count(seconds), self.sample_count(start))

    def normalize(self, samples, peak, *, epsilon=1e-9, additive=False, divide_first=False):
        """Divide by max(peak, epsilon), or by peak + epsilon if additive."""
        from .Internals import buffers

        return buffers.normalize(samples, peak, epsilon, additive, divide_first)

    def linear_envelope(self, t, attack, release):
        from .Internals import envelopes

        return envelopes.linear(t, attack, release)

    def decay_envelope(self, t, attack, decay):
        from .Internals import envelopes

        return envelopes.exponential(t, attack, decay)

    def attack_release(self, count, attack, release, curve=1.0):
        from .Internals import envelopes

        return envelopes.attack_release(count, int(attack * self.sample_rate),
                                        int(release * self.sample_rate), curve)

    def fade(self, samples, attack=0.0, release=0.0, *, curve=1.0, copy=False):
        """Apply edge fades in place, unless copy=True. Times are seconds."""
        from .Internals import envelopes

        return envelopes.fade(samples, self.sample_count(attack), self.sample_count(release), curve, copy)

    def trim_tail(self, samples, floor=0.002):
        from .Internals import envelopes

        return envelopes.trim_tail(samples, floor, int(0.01 * self.sample_rate))

    def lowpass(self, samples, cutoff, order=2):
        from .Internals import filters

        return filters.butterworth(samples, cutoff, "lowpass", order, self.sample_rate)

    def highpass(self, samples, cutoff, order=2):
        from .Internals import filters

        return filters.butterworth(samples, cutoff, "highpass", order, self.sample_rate)

    def bandpass(self, samples, low, high, order=2):
        from .Internals import filters

        return filters.butterworth(samples, (low, high), "bandpass", order, self.sample_rate)

    def one_pole_lowpass(self, samples, cutoff):
        from .Internals import filters

        return filters.one_pole(samples, cutoff, self.sample_rate)

    def one_pole_highpass(self, samples, cutoff):
        return samples - self.one_pole_lowpass(samples, cutoff)

    def one_pole_bandpass(self, samples, low, high):
        return self.one_pole_lowpass(self.one_pole_highpass(samples, low), high)

    def lowpass_samples(self, samples, cutoff):
        """Scalar/list one-pole algorithm; requires only the standard library."""
        from .Internals import scalar

        return scalar.lowpass(samples, cutoff, self.sample_rate)

    def sweep_filter(self, samples, kind, cutoff, order=2, width=None, *, block_size=256):
        from .Internals import filters

        return filters.sweep_filter(samples, kind, cutoff, order, width, block_size, self.sample_rate)

    def swept_filter(self, samples, cutoff_at, *, kind="lowpass", order=2, blocks=64):
        """Overlap-add Hann windows; cutoff_at(seconds) returns Hz or a pair."""
        from .Internals import filters

        return filters.swept(samples, cutoff_at, kind, order, blocks, self.sample_rate)

    def sweep_bandpass(self, samples, start, end, width, *, blocks=64):
        from .Internals import filters

        return filters.sweep_bandpass(samples, start, end, width, blocks, self.sample_rate)

    def formant_voice(self, source, formants):
        """Sum swept bands described by (center curve, width ratio, gain)."""
        from .Internals import filters

        return filters.formant_voice(source, formants, self.sample_rate)

    def noise(self, seconds, count=None):
        return self.rng.uniform(-1.0, 1.0, self.sample_count(seconds) if count is None else count)

    def brown(self, seconds):
        from .Internals import oscillators

        return oscillators.brown(self.rng, self.sample_count(seconds))

    def smooth_random(self, seconds, rate, low=0.0, high=1.0):
        from .Internals import oscillators

        return oscillators.smooth_random(self.rng, self.sample_count(seconds), seconds, rate, low, high)

    def saw_phase(self, phase, harmonics=40):
        """Fixed harmonic sum from phase; does not enforce a Nyquist limit."""
        from .Internals import oscillators

        return oscillators.saw_phase(phase, harmonics)

    def saw(self, frequency, t, harmonics=24):
        """Harmonic sum with the existing fixed-frequency Nyquist cutoff."""
        from .Internals import oscillators

        return oscillators.saw(frequency, t, harmonics, self.sample_rate)

    def saw_sample(self, phase, harmonics):
        """Unscaled scalar harmonic sum; standard library only."""
        from .Internals import scalar

        return scalar.saw(phase, harmonics)

    def pulse(self, t, frequency, width=0.3):
        from .Internals import oscillators

        return oscillators.pulse(t, frequency, width)

    @staticmethod
    def sawtooth(phase):
        """Existing SciPy sawtooth; intentionally not band limited."""
        from .Internals import oscillators

        return oscillators.sawtooth(phase)

    @staticmethod
    def square(phase):
        """Existing SciPy square wave; intentionally not band limited."""
        from .Internals import oscillators

        return oscillators.square(phase)

    def crackles(self, seconds, per_second, decay, color_low, color_high, gain_curve=None):
        from .Internals import textures

        return textures.crackles(self.rng, seconds, per_second, decay, color_low, color_high,
                                 gain_curve, self.sample_rate)

    def pings(self, seconds, per_second, low, high, decay, gain_curve=None):
        from .Internals import textures

        return textures.pings(self.rng, seconds, per_second, low, high, decay, gain_curve, self.sample_rate)

    def reverb(self, samples, seconds=1.6, mix=0.25, damp=3500.0, predelay=0.012):
        from .Internals import effects

        return effects.reverb(samples, self.rng, self.sample_count(seconds), seconds, mix, damp,
                              self.sample_count(predelay), self.sample_rate)

    def saturate(self, samples, drive):
        from .Internals import effects

        return effects.saturate(samples, drive)

    def read_source(self, path):
        """Decode, downmix, resample and remove DC. Requires soundfile."""
        from .Internals import wave_io

        return wave_io.read_source(path, self.sample_rate)

    def write_wav(self, path, samples, *, rounding=False, clip=True):
        """Write mono PCM16; rounding=True preserves the scalar quantizer."""
        from .Internals import wave_io

        wave_io.write_wav(path, samples, self.sample_rate, rounding, clip)

    @staticmethod
    def run(sounds, write, *, prefix="", names=None, validate=True):
        """Select named sounds from argv (or names), call write(name), print filenames."""
        from .Internals import cli

        cli.run(sounds, write, prefix, names, validate)
