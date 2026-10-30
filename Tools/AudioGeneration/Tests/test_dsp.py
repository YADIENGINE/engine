"""EQ, vowels, waveshaping, rooms, dynamics, time/pitch, unison, tuning and loops through the public API."""

from pathlib import Path
import sys
import tempfile
import unittest

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from AudioGeneration import SoundGenerator


def level_db(samples):
    return 20 * np.log10(np.sqrt(np.mean(samples ** 2)))


def peak_frequency(samples, rate, low=0.0, high=None):
    magnitude = np.abs(np.fft.rfft(samples * np.hanning(len(samples))))
    frequencies = np.fft.rfftfreq(len(samples), 1 / rate)
    band = (frequencies >= low) & (frequencies <= (high or rate / 2))
    return frequencies[band][np.argmax(magnitude[band])]


class ToneShapingTests(unittest.TestCase):
    def setUp(self):
        self.rate = 44100
        self.g = SoundGenerator(self.rate, seed=3)

    def test_bell_and_shelves_change_only_their_region(self):
        settle = slice(4000, None)
        for frequency, filtered, expected in (
                (1000.0, lambda x: self.g.bell(x, 1000, 6.0, q=2), 6.0),
                (8000.0, lambda x: self.g.bell(x, 1000, 6.0, q=2), 0.0),
                (80.0, lambda x: self.g.low_shelf(x, 400, -12.0), -12.0),
                (6000.0, lambda x: self.g.low_shelf(x, 400, -12.0), 0.0),
                (9000.0, lambda x: self.g.high_shelf(x, 2000, 5.0), 5.0),
                (100.0, lambda x: self.g.high_shelf(x, 2000, 5.0), 0.0)):
            tone = self.g.sine(frequency, 0.5)
            self.assertAlmostEqual(level_db(filtered(tone)[settle]) - level_db(tone[settle]), expected, delta=0.5)
        with self.assertRaises(ValueError):
            self.g.bell(np.zeros(10), 30000, 3.0)

    def test_vowels_place_formants_and_glide(self):
        buzz = self.g.saw(110.0, 1.0)
        a = self.g.vowel(buzz, "a")
        i = self.g.vowel(buzz, "i")
        self.assertGreater(peak_frequency(a, self.rate, 500, 1000), 600)
        self.assertLess(peak_frequency(i, self.rate, 150, 1000), 400)
        glide = self.g.vowel(buzz, [(0.0, "a"), (1.0, "u")])
        self.assertEqual(len(glide), len(buzz))
        bigger = self.g.vowel(buzz, "a", scale=0.7)
        self.assertLess(peak_frequency(bigger, self.rate, 300, 1000), peak_frequency(a, self.rate, 300, 1000))
        with self.assertRaises(ValueError):
            self.g.vowel(buzz, "y")

    def test_waveshapers_stay_bounded_and_bias_adds_even_harmonics(self):
        tone = self.g.sine(200.0, 0.5)
        for shape in ("tanh", "clip", "fold"):
            shaped = self.g.saturate(tone, 3.0, shape=shape, bias=0.3)
            self.assertLess(np.abs(shaped).max(), 1.05)
            self.assertEqual(len(shaped), len(tone))
        symmetric = np.abs(np.fft.rfft(self.g.saturate(tone, 3.0)))
        asymmetric = np.abs(np.fft.rfft(self.g.saturate(tone, 3.0, bias=0.3)))
        second = round(400 * len(tone) / self.rate)
        self.assertGreater(asymmetric[second - 2:second + 3].max(), 20 * symmetric[second - 2:second + 3].max())
        np.testing.assert_allclose(self.g.saturate(np.zeros(100), 3.0, shape="fold", bias=0.4), 0.0, atol=1e-12)

    def test_unison_spreads_and_balances_voices(self):
        single = self.g.unison(220.0, 1.0, voices=1)
        stack = self.g.unison(220.0, 1.0, voices=7, detune=30.0)
        self.assertAlmostEqual(level_db(stack), level_db(single), delta=3.0)
        magnitude = np.abs(np.fft.rfft(stack))
        frequencies = np.fft.rfftfreq(len(stack), 1 / self.rate)
        near = magnitude[(frequencies > 210) & (frequencies < 230)]
        self.assertGreater(np.count_nonzero(near > 0.3 * near.max()), 3)
        with self.assertRaises(ValueError):
            self.g.unison(220.0, 1.0, wave="noise")

    def test_pluck_is_tuned_even_for_high_notes(self):
        for frequency in (110.0, 1760.0, 3520.0):
            string = self.g.pluck(frequency, 1.0, decay=2.0)[2000:30000]
            self.assertAlmostEqual(peak_frequency(string, self.rate, frequency * 0.9, frequency * 1.1),
                                   frequency, delta=2.0)


class RoomTests(unittest.TestCase):
    def setUp(self):
        self.rate = 44100
        self.g = SoundGenerator(self.rate, seed=4)

    def test_highs_decay_faster_than_lows(self):
        def treble_share(samples):
            magnitude = np.abs(np.fft.rfft(samples))
            frequencies = np.fft.rfftfreq(len(samples), 1 / self.rate)
            return magnitude[frequencies > 4000].sum() / magnitude[frequencies < 1000].sum()

        tail = self.g.reverb(np.r_[1.0, np.zeros(100)], 2.0, mix=1.0, damp=2000.0)
        self.assertLess(treble_share(tail[50000:70000]), 0.3 * treble_share(tail[2000:12000]))
        self.assertGreater(len(tail), 2 * self.rate)

    def test_recorded_impulse_from_samples_or_file(self):
        click = np.r_[1.0, np.zeros(99)]
        response = np.r_[0.0, 1.0, 0.0, -0.5]
        wet = self.g.reverb(click, impulse=response, mix=1.0, predelay=0.0)
        np.testing.assert_allclose(wet[:4], response / np.linalg.norm(response), atol=1e-9)
        with tempfile.TemporaryDirectory() as directory:
            path = self.g.save(Path(directory) / "room.wav", np.r_[0.5, np.zeros(10), 0.25])
            self.assertEqual(len(self.g.reverb(click, impulse=path, predelay=0.0)), 100 + 12 - 1)
        with self.assertRaises(ValueError):
            self.g.reverb(click, impulse=np.zeros(5))


class DynamicsTests(unittest.TestCase):
    def setUp(self):
        self.rate = 44100
        self.g = SoundGenerator(self.rate, seed=5)
        self.tone = self.g.sine(500.0, 0.5)

    def test_compressor_reduces_loud_parts_more_than_quiet_ones(self):
        loud = self.g.compress(self.tone, threshold_db=-20, ratio=4, knee_db=0)[5000:]
        quiet = self.g.compress(self.tone * 0.05, threshold_db=-20, ratio=4, knee_db=0)[5000:]
        self.assertAlmostEqual(level_db(loud) - level_db(self.tone[5000:]), -15.0, delta=1.0)
        self.assertAlmostEqual(level_db(quiet) - level_db(self.tone[5000:] * 0.05), 0.0, delta=0.1)
        with self.assertRaises(ValueError):
            self.g.compress(self.tone, ratio=0.5)

    def test_limiter_holds_the_ceiling_and_leaves_quiet_audio_alone(self):
        burst = np.r_[self.tone * 0.2, self.tone * 3.0, self.tone * 0.2]
        limited = self.g.limit(burst, ceiling=0.8)
        self.assertLessEqual(np.abs(limited).max(), 0.8 + 1e-12)
        np.testing.assert_allclose(limited[:20000], burst[:20000])
        self.assertGreater(np.abs(limited[len(self.tone):2 * len(self.tone)]).max(), 0.79)

    def test_gate_closes_on_quiet_tails(self):
        signal = np.r_[self.tone, self.tone * 0.001]
        gated = self.g.gate(signal, threshold_db=-40, release=0.02)
        self.assertLess(np.abs(gated[-5000:]).max(), 1e-5)
        self.assertGreater(np.abs(gated[5000:20000]).max(), 0.99)

    def test_transient_shaper_boosts_onsets_only(self):
        hit = self.g.thump(0.5, 200.0, 60.0, decay=0.2)
        shaped = self.g.shape_transients(hit, attack_db=9.0)
        self.assertGreater(np.abs(shaped[:400]).max(), 1.5 * np.abs(hit[:400]).max())
        np.testing.assert_allclose(shaped[15000:], hit[15000:], atol=0.02)


class TimeTests(unittest.TestCase):
    def setUp(self):
        self.rate = 44100
        self.g = SoundGenerator(self.rate, seed=6)

    def test_stretch_keeps_pitch_and_scales_duration(self):
        tone = self.g.sine(440.0, 0.5)
        for factor in (0.5, 1.5, 2.0):
            stretched = self.g.stretch(tone, factor)
            self.assertEqual(len(stretched), round(len(tone) * factor))
            middle = stretched[len(stretched) // 4:3 * len(stretched) // 4]
            self.assertAlmostEqual(peak_frequency(middle, self.rate), 440.0, delta=5.0)
            self.assertAlmostEqual(np.abs(middle).max(), 1.0, delta=0.1)

    def test_pitch_shift_keeps_duration(self):
        tone = self.g.sine(500.0, 1.0)
        for semitones, expected in ((12, 1000.0), (-12, 250.0), (7, 500.0 * 2 ** (7 / 12))):
            shifted = self.g.pitch_shift(tone, semitones)
            self.assertEqual(len(shifted), len(tone))
            self.assertAlmostEqual(peak_frequency(shifted[5000:-5000], self.rate), expected, delta=expected * 0.01)

    def test_loop_is_continuous_across_the_seam(self):
        drone = self.g.lowpass(self.g.noise(2.0, color="pink"), 800)
        looped = self.g.loop(drone, 0.2)
        self.assertEqual(len(looped), len(drone) - round(0.2 * self.rate))
        seam = abs(looped[0] - looped[-1])
        typical = np.median(np.abs(np.diff(looped)))
        self.assertLess(seam, 10 * typical)
        with self.assertRaises(ValueError):
            self.g.loop(drone, 1.5)


if __name__ == "__main__":
    unittest.main()
