"""Filters, effects, timeline and level operations through the public API."""

from pathlib import Path
import sys
import unittest

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from AudioGeneration import SoundGenerator


def rms(samples):
    return float(np.sqrt(np.mean(samples ** 2)))


class FilterTests(unittest.TestCase):
    def setUp(self):
        self.rate = 44100
        self.g = SoundGenerator(self.rate, seed=2)

    def test_fixed_filters_pass_and_reject(self):
        low, high = self.g.sine(200.0, 0.5), self.g.sine(8000.0, 0.5)
        self.assertGreater(rms(self.g.lowpass(low, 1000)[5000:]), 0.65)
        self.assertLess(rms(self.g.lowpass(high, 1000)[5000:]), 0.02)
        self.assertLess(rms(self.g.highpass(low, 2000)[5000:]), 0.01)
        band = self.g.bandpass(self.g.sine(1000.0, 0.5), 700, 1400)
        self.assertGreater(rms(band[5000:]), 0.6)
        centered = self.g.bandpass(self.g.sine(1000.0, 0.5), center=1000, width=1.5)
        self.assertGreater(rms(centered[5000:]), 0.6)
        sweep = self.g.bandpass(self.g.noise(0.5), center=self.g.contour([(0, 500), (0.5, 4000)]), width=1.4)
        self.assertEqual(len(sweep), round(0.5 * self.rate))
        with self.assertRaises(ValueError):
            self.g.bandpass(self.g.noise(0.1), 500, 900, center=700, width=1.2)
        with self.assertRaises(ValueError):
            self.g.bandpass(self.g.noise(0.1), center=700, width=0.5)

    def test_moving_cutoff_tracks_the_curve_without_clicks(self):
        tone = self.g.sine(3000.0, 1.0)
        cutoff = self.g.envelope([(0, 300), (0.5, 300), (0.5, 15000), (1, 15000)], 1.0)
        out = self.g.lowpass(tone, cutoff, order=4)
        self.assertLess(rms(out[10000:20000]), 0.01)
        self.assertGreater(rms(out[30000:]), 0.65)
        self.assertLess(np.abs(np.diff(out)).max(), 0.5)
        with self.assertRaises(ValueError):
            self.g.lowpass(tone, cutoff, order=3)

    def test_resonator_has_unity_peak_and_follows_a_center_curve(self):
        tone = self.g.sine(1000.0, 0.5)
        self.assertAlmostEqual(rms(self.g.resonate(tone, 1000.0, q=10)[10000:]), rms(tone), delta=0.02)
        self.assertLess(rms(self.g.resonate(tone, 4000.0, q=10)[10000:]), 0.05)
        moving = self.g.bandpass(tone, np.full(len(tone), 700.0), np.full(len(tone), 1400.0))
        self.assertGreater(rms(moving[10000:]), 0.6)
        voice = self.g.formants(self.g.saw(110.0, 0.5), [(700, 6, 1.0), (1200, 8, 0.5)])
        self.assertEqual(len(voice), len(tone))

    def test_dc_block_removes_offset(self):
        self.assertLess(abs(np.mean(self.g.dc_block(self.g.noise(1.0) * 0.1 + 0.5)[20000:])), 0.01)


class EffectTests(unittest.TestCase):
    def setUp(self):
        self.rate = 8000
        self.g = SoundGenerator(self.rate, seed=3)
        self.click = np.r_[1.0, np.zeros(799)]

    def test_saturation_is_bounded_bypassable_and_aligned(self):
        tone = self.g.sine(200.0, 0.2)
        driven = self.g.saturate(tone, 4.0)
        self.assertEqual(len(driven), len(tone))
        self.assertLess(np.abs(driven).max(), 1.05)
        np.testing.assert_array_equal(self.g.saturate(tone, 0.0), tone)
        self.assertGreater(np.corrcoef(driven, tone)[0, 1], 0.9)
        with self.assertRaises(ValueError):
            self.g.saturate(tone, 2.0, oversample=3)

    def test_delay_places_decaying_repeats_and_keeps_the_tail(self):
        out = self.g.delay(self.click, 0.01, feedback=0.5, mix=0.5)
        self.assertAlmostEqual(out[0], 0.5)
        self.assertAlmostEqual(out[80], 0.5)
        self.assertAlmostEqual(out[160], 0.25)
        self.assertGreater(len(out), len(self.click))
        with self.assertRaises(ValueError):
            self.g.delay(self.click, 0.01, feedback=1.0)

    def test_reverb_tail_chorus_speed_bitcrush_and_follow(self):
        steady = self.g.noise(1.0) * 0.5
        wet = self.g.reverb(steady, 0.5, mix=1.0)
        self.assertEqual(len(wet), len(steady) + round((0.5 + 0.012) * self.rate) - 1)
        self.assertAlmostEqual(rms(wet[2000:7000]), rms(steady), delta=0.08)
        self.assertEqual(len(self.g.chorus(steady)), len(steady))
        self.assertEqual(len(self.g.speed(steady, 2.0)), 4000)
        self.assertEqual(len(self.g.speed(steady, 0.5)), 16000)
        crushed = self.g.bitcrush(steady, bits=3, rate=1000)
        self.assertLessEqual(len(np.unique(crushed)), 9)
        self.assertTrue(np.all(crushed[:8] == crushed[0]))
        envelope = self.g.follow(np.r_[np.ones(800), np.zeros(800)], attack=0.001, release=0.01)
        self.assertGreater(envelope[700], 0.99)
        self.assertLess(envelope[-1], 0.01)


class TimelineTests(unittest.TestCase):
    def setUp(self):
        self.g = SoundGenerator(1000, seed=4)

    def test_mix_places_crops_and_rejects_negative_starts(self):
        out = self.g.mix([np.ones(3), (0.005, np.ones(3), 0.5)])
        np.testing.assert_allclose(out, [1, 1, 1, 0, 0, 0.5, 0.5, 0.5])
        self.assertEqual(len(self.g.mix([(0.01, np.ones(5))], seconds=0.012)), 12)
        self.assertEqual(len(self.g.mix({"late": (1.0, np.ones(5))}, seconds=0.01)), 10)
        with self.assertRaises(ValueError):
            self.g.mix([(-0.001, np.ones(3))])
        with self.assertRaises(ValueError):
            self.g.mix([])

    def test_pad_and_segment(self):
        self.assertEqual(len(self.g.pad(np.ones(3), 0.002)), 5)
        np.testing.assert_array_equal(self.g.segment(np.arange(5.0), 0.004, 0.003), [3, 4, 0, 0])


class LevelTests(unittest.TestCase):
    def setUp(self):
        self.rate = 48000
        self.g = SoundGenerator(self.rate, seed=5)

    def test_loudness_matches_the_bs1770_sine_reference(self):
        self.assertAlmostEqual(self.g.loudness(self.g.sine(997.0, 2.0)), -3.01, delta=0.02)
        self.assertAlmostEqual(self.g.loudness(self.g.sine(997.0, 2.0) * 0.1), -23.01, delta=0.02)
        self.assertIsNone(self.g.loudness(np.zeros(100)))
        # Gating ignores the silent tail (ungated it would be 6 dB lower).
        quiet_gap = np.r_[self.g.sine(997.0, 1.0), np.zeros(self.rate * 3)]
        self.assertGreater(self.g.loudness(quiet_gap), -4.5)

    def test_normalize_by_peak_or_loudness(self):
        noise = self.g.noise(1.0)
        self.assertAlmostEqual(np.abs(self.g.normalize(noise, peak=0.5)).max(), 0.5)
        self.assertAlmostEqual(self.g.loudness(self.g.normalize(noise, lufs=-18)), -18, places=6)
        np.testing.assert_array_equal(self.g.normalize(np.zeros(4), peak=0.5), np.zeros(4))
        with self.assertRaises(ValueError):
            self.g.normalize(noise)

    def test_trim_keeps_late_reflections_and_fades(self):
        source = np.r_[np.ones(4800), np.zeros(9600), np.full(480, 0.01), np.zeros(48000)]
        trimmed = self.g.trim(source)
        self.assertGreater(len(trimmed), 4800 + 9600 + 480)
        self.assertLess(len(trimmed), len(source) / 2)
        self.assertEqual(trimmed[-1], 0.0)
        self.assertEqual(len(self.g.trim(np.zeros(100))), 0)

    def test_finish_chain(self):
        raw = np.r_[self.g.noise(0.2) + 0.4, np.zeros(self.rate)]
        done = self.g.finish(raw, peak=0.8)
        self.assertLess(len(done), len(raw))
        self.assertLess(abs(np.mean(done[:4800])), 0.1)
        self.assertLessEqual(np.abs(done).max(), 0.8 + 1e-9)
        self.assertEqual(done[0], 0.0)
        self.assertEqual(done[-1], 0.0)
        loud = self.g.finish(self.g.noise(0.5), peak=None, lufs=0.0, ceiling=0.9)
        self.assertAlmostEqual(np.abs(loud).max(), 0.9)
        self.assertEqual(len(self.g.finish(raw, trim=None)), len(raw))
        with self.assertRaises(ValueError):
            self.g.finish(raw, lufs=-14)
        with self.assertRaises(ValueError):
            self.g.finish(np.array([0.0, np.nan]))


if __name__ == "__main__":
    unittest.main()
