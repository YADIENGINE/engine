"""Spectral and temporal behavior of the opt-in quality paths."""

from pathlib import Path
import sys
import unittest

import numpy as np

ENGINE_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ENGINE_ROOT))

from AudioGeneration import SoundGenerator


def nonharmonic_energy(samples, sample_rate, fundamental, odd=False):
    power = np.abs(np.fft.rfft(samples)) ** 2
    frequencies = np.fft.rfftfreq(len(samples), 1 / sample_rate)
    mask = np.ones(len(power), dtype=bool)
    mask[0] = False
    bin_width = sample_rate / len(samples)
    for harmonic in range(1, int(sample_rate / 2 / fundamental) + 1, 2 if odd else 1):
        mask[np.abs(frequencies - fundamental * harmonic) <= bin_width] = False
    return float(power[mask].sum() / power.sum())


class SignalQualityTests(unittest.TestCase):
    def test_additive_oscillators_reject_folded_harmonics(self):
        g = SoundGenerator(48000)
        frequency = 7000
        t = g.time_axis(0.2)
        phase = 2 * np.pi * frequency * t
        for clean, naive, odd in ((g.bandlimited_saw(frequency, 0.2), g.sawtooth(phase), False),
                                  (g.bandlimited_square(frequency, 0.2), g.square(phase), True)):
            self.assertEqual(len(clean), len(t))
            self.assertGreater(np.std(clean), 0.3)
            self.assertLess(nonharmonic_energy(clean, g.sample_rate, frequency, odd),
                            nonharmonic_energy(naive, g.sample_rate, frequency, odd) * 1e-4)

    def test_pitch_curves_phase_and_near_nyquist_fundamental(self):
        g = SoundGenerator(8000)
        curve = np.geomspace(80, 3000, 400)
        before = curve.copy()
        self.assertTrue(np.isfinite(g.bandlimited_saw(curve)).all())
        np.testing.assert_array_equal(curve, before)
        np.testing.assert_allclose(g.bandlimited_square(np.full(400, 1000.0)),
                                   g.bandlimited_square(1000, 0.05), atol=1e-12)
        np.testing.assert_allclose(g.bandlimited_saw(220, 0.02, phase=0.3),
                                   g.bandlimited_saw(220, 0.02, phase=0.3 + 200 * np.pi), atol=1e-10)
        near_nyquist = g.bandlimited_saw(3900, 0.05)
        self.assertGreater(np.std(near_nyquist), 0.3)
        self.assertEqual(len(g.bandlimited_saw(np.array([]))), 0)

    def test_oversampling_reduces_distortion_aliases(self):
        g = SoundGenerator(48000)
        frequency = 7000
        samples = 0.8 * np.sin(2 * np.pi * frequency * g.time_axis(0.2))
        before = samples.copy()
        naive = g.saturate(samples, 4)
        clean = g.saturate(samples, 4, oversample=4)
        # Exclude endpoint transients; the remaining region contains complete periods.
        region = slice(480, -480)
        self.assertLess(nonharmonic_energy(clean[region], g.sample_rate, frequency, True),
                        nonharmonic_energy(naive[region], g.sample_rate, frequency, True) * 0.01)
        self.assertEqual(clean.shape, samples.shape)
        np.testing.assert_array_equal(samples, before)

    def test_oversampled_distortion_keeps_alignment_and_handles_short_buffers(self):
        g = SoundGenerator()
        impulse = np.zeros(321)
        impulse[160] = 0.8
        result = g.saturate(impulse, 3, oversample=4)
        self.assertEqual(np.argmax(np.abs(result)), 160)
        self.assertEqual(len(result), len(impulse))
        for factor in (2, 4, 8, 16):
            self.assertEqual(len(g.saturate([], 2, oversample=factor)), 0)
            self.assertTrue(np.isfinite(g.saturate([0.2], 2, oversample=factor)).all())
            np.testing.assert_allclose(g.saturate(np.full(100, 0.2), 2, oversample=factor),
                                       np.tanh(0.4) / np.tanh(2), atol=1e-5)
        np.testing.assert_array_equal(g.saturate(impulse, 0, oversample=4), impulse)

    def test_smooth_filter_has_expected_frequency_response(self):
        g = SoundGenerator(8000)
        t = g.time_axis(0.2)
        low = np.sin(2 * np.pi * 100 * t)
        high = np.sin(2 * np.pi * 3000 * t)
        region = slice(len(t) // 2, None)
        self.assertGreater(np.std(g.smooth_filter(low, 500)[region]),
                           10 * np.std(g.smooth_filter(high, 500)[region]))
        self.assertGreater(np.std(g.smooth_filter(high, 500, kind="highpass")[region]),
                           10 * np.std(g.smooth_filter(low, 500, kind="highpass")[region]))

    def test_filter_parameter_changes_preserve_dc_and_can_be_smoothed(self):
        g = SoundGenerator(8000)
        count = 4000
        cutoff = np.r_[np.full(count // 2, 200), np.full(count // 2, 3000)]
        dc = g.smooth_filter(np.ones(count), cutoff, smoothing_seconds=0)
        np.testing.assert_allclose(dc[count // 2:], 1, atol=1e-10)
        tone = np.sin(2 * np.pi * 2000 * np.arange(count) / g.sample_rate)
        instant = g.smooth_filter(tone, cutoff, smoothing_seconds=0)
        smoothed = g.smooth_filter(tone, cutoff, smoothing_seconds=0.02)
        onset = slice(count // 2, count // 2 + 24)
        self.assertLess(np.std(smoothed[onset]), np.std(instant[onset]) * 0.5)
        for kind in ("lowpass", "highpass", "bandpass"):
            result = g.smooth_filter(tone, np.geomspace(20, 3800, count), kind=kind, q=2)
            self.assertTrue(np.isfinite(result).all())
            self.assertEqual(result.shape, tone.shape)

    def test_energy_trim_retains_late_reflections_and_fades_after_them(self):
        g = SoundGenerator(1000)
        samples = np.zeros(1000)
        samples[:100] = 0.3
        samples[400:420] = 0.01
        samples[700:] = 1e-6
        before = samples.copy()
        trimmed = g.trim_tail_energy(samples, threshold_dbfs=-50, hold_seconds=0.02)
        self.assertGreater(len(trimmed), 420)
        self.assertLess(len(trimmed), 700)
        np.testing.assert_array_equal(trimmed[:420], samples[:420])
        self.assertEqual(trimmed[-1], 0)
        np.testing.assert_array_equal(samples, before)
        self.assertEqual(len(g.trim_tail_energy(np.zeros(100))), 0)
        self.assertEqual(len(g.trim_tail_energy(np.zeros(100), min_seconds=0.02)), 20)

    def test_energy_peak_guard_preserves_sparse_transients(self):
        g = SoundGenerator(1000)
        samples = np.zeros(1000)
        samples[505] = 0.02
        trimmed = g.trim_tail_energy(samples, threshold_dbfs=-50, window_seconds=0.1,
                                     hold_seconds=0, fade_seconds=0)
        self.assertGreater(len(trimmed), 505)
        self.assertEqual(trimmed[505], samples[505])

    def test_quality_paths_reject_invalid_controls(self):
        g = SoundGenerator(8000)
        calls = [lambda: g.bandlimited_saw(0, 0.1),
                 lambda: g.bandlimited_square(4000, 0.1),
                 lambda: g.bandlimited_saw([100, 200], seconds=0.1),
                 lambda: g.saturate([0.2], 2, oversample=3),
                 lambda: g.saturate([np.nan], 2, oversample=4),
                 lambda: g.smooth_filter([0], 4000),
                 lambda: g.smooth_filter([0], [100, 200]),
                 lambda: g.trim_tail_energy([np.inf]),
                 lambda: g.trim_tail_energy([0], window_seconds=0)]
        for call in calls:
            with self.assertRaises(ValueError):
                call()


if __name__ == "__main__":
    unittest.main()
