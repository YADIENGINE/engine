"""Control curves and sources through the public API: shapes, band limits and validation."""

from pathlib import Path
import sys
import unittest

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from AudioGeneration import SoundGenerator


def spectrum(samples, rate):
    magnitude = np.abs(np.fft.rfft(samples * np.hanning(len(samples))))
    return np.fft.rfftfreq(len(samples), 1 / rate), magnitude


def dominant(samples, rate):
    frequencies, magnitude = spectrum(samples, rate)
    return frequencies[np.argmax(magnitude)]


class CurveTests(unittest.TestCase):
    def setUp(self):
        self.g = SoundGenerator(1000, seed=1)

    def test_envelope_passes_through_points_and_holds_the_ends(self):
        env = self.g.envelope([(0.1, 0.0), (0.2, 1.0), (0.4, 0.5)], 0.5)
        self.assertEqual(len(env), 500)
        self.assertEqual(env[50], 0.0)
        self.assertAlmostEqual(env[150], 0.5)
        self.assertAlmostEqual(env[200], 1.0)
        self.assertAlmostEqual(env[499], 0.5)
        fast = self.g.envelope([(0, 0), (1, 1)], curve=0.5)
        slow = self.g.envelope([(0, 0), (1, 1)], curve=2.0)
        self.assertGreater(fast[250], 0.25)
        self.assertLess(slow[250], 0.25)
        with self.assertRaises(ValueError):
            self.g.envelope([(0.2, 1), (0.1, 0)])

    def test_contour_is_geometric(self):
        hz = self.g.contour([(0, 100), (1, 400)])
        self.assertAlmostEqual(hz[500], 200.0, delta=0.5)
        with self.assertRaises(ValueError):
            self.g.contour([(0, 0), (1, 1)])

    def test_adsr_reaches_peak_sustain_and_ends_silent(self):
        env = self.g.adsr(1.0, 0.1, 0.1, 0.4, 0.2)
        self.assertAlmostEqual(env[100], 1.0)
        self.assertAlmostEqual(env[500], 0.4)
        self.assertAlmostEqual(env[-1], 0.0, places=2)
        squeezed = self.g.adsr(0.1, 1.0, 1.0, 0.5, 1.0)
        self.assertEqual(len(squeezed), 100)
        self.assertLessEqual(squeezed.max(), 1.0)

    def test_decay_glide_lfo_and_random_curve(self):
        decay = self.g.decay(1.0, 0.2, attack=0.01)
        self.assertEqual(decay[0], 0.0)
        self.assertAlmostEqual(decay[210] / decay[10], np.exp(-1.0), places=3)
        glide = self.g.glide(800, 100, 1.0, 0.05)
        self.assertAlmostEqual(glide[0], 800)
        self.assertAlmostEqual(glide[-1], 100, delta=0.01)
        for shape in ("sine", "triangle", "square", "saw"):
            wave = self.g.lfo(1.0, 3.0, shape=shape)
            self.assertLessEqual(np.abs(wave).max(), 1.0 + 1e-12)
        curve = self.g.random_curve(2.0, 8.0, low=2.0, high=3.0)
        self.assertTrue(np.all((curve >= 2.0) & (curve <= 3.0)))
        self.assertLess(np.abs(np.diff(curve)).max(), 0.1)

    def test_decay_hold_rise_and_lfo_center(self):
        held = self.g.decay(1.0, 0.1, attack=0.1, hold=0.2)
        self.assertAlmostEqual(held[50], 0.5)
        self.assertEqual(held[250], 1.0)
        self.assertAlmostEqual(held[400], np.exp(-1.0), places=3)
        ramp = self.g.rise(0.5, 0.2)
        self.assertAlmostEqual(ramp[100], 0.5)
        self.assertEqual(ramp[-1], 1.0)
        self.assertEqual(len(ramp), 500)
        vibrato = self.g.lfo(1.0, 5.0, center=1.0, depth=0.02)
        self.assertAlmostEqual(vibrato.max(), 1.02, places=4)
        self.assertAlmostEqual(vibrato.min(), 0.98, places=4)

    def test_fade_returns_a_new_array_with_silent_edges(self):
        source = np.ones(100)
        faded = self.g.fade(source, 0.01, 0.02)
        self.assertEqual(source.min(), 1.0)
        self.assertEqual(faded[0], 0.0)
        self.assertEqual(faded[-1], 0.0)
        self.assertEqual(faded[50], 1.0)

    def test_note_names_and_midi(self):
        self.assertAlmostEqual(SoundGenerator.note("A4"), 440.0)
        self.assertAlmostEqual(SoundGenerator.note("A5"), 880.0)
        self.assertAlmostEqual(SoundGenerator.note("C#5"), SoundGenerator.note(73))
        self.assertAlmostEqual(SoundGenerator.note("Db5"), SoundGenerator.note("C#5"))
        with self.assertRaises(ValueError):
            SoundGenerator.note("H2")


class OscillatorTests(unittest.TestCase):
    def setUp(self):
        self.rate = 44100
        self.g = SoundGenerator(self.rate, seed=1)

    def test_phase_starts_exactly_and_integrates(self):
        phase = self.g.phase(441.0, 0.01, start=0.5)
        self.assertEqual(phase[0], 0.5)
        self.assertAlmostEqual(phase[100] - phase[0], 2 * np.pi)

    def test_waveforms_have_the_requested_pitch_and_no_dc(self):
        for wave in (self.g.sine, self.g.saw, self.g.square, self.g.triangle, self.g.pulse):
            samples = wave(220.0, 0.5)
            self.assertAlmostEqual(dominant(samples, self.rate), 220.0, delta=2.5)
            self.assertLess(abs(np.mean(samples)), 0.01)
        narrow = self.g.pulse(220.0, 0.5, width=0.2)
        self.assertLess(abs(np.mean(narrow)), 0.01)

    def test_band_limited_waves_leave_the_guard_band_empty(self):
        for wave in (self.g.saw, self.g.square, self.g.triangle):
            frequencies, magnitude = spectrum(wave(3100.0, 0.5), self.rate)
            guard = magnitude[frequencies > 0.96 * self.rate / 2].max()
            self.assertLess(guard / magnitude.max(), 1e-3)
        sweep = self.g.saw(self.g.contour([(0, 200), (0.5, 8000)]))
        frequencies, magnitude = spectrum(sweep, self.rate)
        self.assertLess(magnitude[frequencies > 0.96 * self.rate / 2].max() / magnitude.max(), 1e-3)

    def test_frequency_validation(self):
        with self.assertRaises(ValueError):
            self.g.saw(440.0)
        with self.assertRaises(ValueError):
            self.g.sine(30000.0, 0.1)
        with self.assertRaises(ValueError):
            self.g.saw(np.full(10, 440.0), 1.0)
        self.assertEqual(len(self.g.saw(np.full(10, 440.0))), 10)

    def test_fm_index_adds_sidebands(self):
        plain = self.g.fm(500.0, 0.5, index=0.0)
        rich = self.g.fm(500.0, 0.5, ratio=1.0, index=3.0)
        np.testing.assert_allclose(plain, self.g.sine(500.0, 0.5), atol=1e-9)
        frequencies, magnitude = spectrum(rich, self.rate)
        self.assertGreater(magnitude[np.abs(frequencies - 1500) < 5].max(), 0.1 * magnitude.max())

    def test_noise_colors_tilt_their_spectrum(self):
        def slope(color):
            frequencies, magnitude = spectrum(self.g.noise(2.0, color=color), self.rate)
            low = np.mean(magnitude[(frequencies > 100) & (frequencies < 200)] ** 2)
            high = np.mean(magnitude[(frequencies > 6400) & (frequencies < 12800)] ** 2)
            return 10 * np.log10(high / low)

        self.assertAlmostEqual(slope("white"), 0, delta=2)
        self.assertAlmostEqual(slope("pink"), -18, delta=3)
        self.assertAlmostEqual(slope("brown"), -36, delta=4)
        for color in ("white", "pink", "brown"):
            self.assertLessEqual(np.abs(self.g.noise(0.2, color=color)).max(), 1.0)

    def test_modal_partials_ring_and_drop_above_nyquist(self):
        bell = self.g.modal(1.0, 500.0, (1.0, 2.76, 100.0), 0.3)
        frequencies, magnitude = spectrum(bell, self.rate)
        self.assertAlmostEqual(frequencies[np.argmax(magnitude)], 500.0, delta=3)
        self.assertGreater(magnitude[np.abs(frequencies - 1380) < 5].max(), 0.05 * magnitude.max())
        self.assertLess(np.abs(bell[-1000:]).max(), np.abs(bell[:5000]).max() * 0.05)
        with self.assertRaises(ValueError):
            self.g.modal(0.1, 500.0, (1.0, 2.0), (0.1,))

    def test_modal_spread_varies_each_strike_and_click_adds_contact_noise(self):
        plain = self.g.modal(0.3, 800.0, (1.0, 2.4), 0.1, random_phase=False)
        detuned = [self.g.modal(0.3, 800.0, (1.0, 2.4), 0.1, random_phase=False, spread=0.05) for _ in range(2)]
        self.assertFalse(np.array_equal(detuned[0], detuned[1]))
        self.assertAlmostEqual(dominant(detuned[0], self.rate), 800.0, delta=45)
        clicked = self.g.modal(0.3, 800.0, (1.0, 2.4), 0.1, random_phase=False, click=0.8)
        frequencies, magnitude = spectrum(clicked[:300], self.rate)
        _, plain_magnitude = spectrum(plain[:300], self.rate)
        high = frequencies > 3000
        self.assertGreater(magnitude[high].sum(), 5 * plain_magnitude[high].sum())

    def test_burst_and_crackle(self):
        hit = self.g.burst(0.3, 0.02, band=(1000, 3000))
        self.assertEqual(len(hit), round(0.3 * self.rate))
        self.assertLess(np.abs(hit[-2000:]).max(), 0.01 * np.abs(hit[:2000]).max())
        self.assertAlmostEqual(dominant(self.g.burst(1.0, 10.0, band=(1800, 2200)), self.rate), 2000, delta=300)
        with self.assertRaises(ValueError):
            self.g.burst(0.1, 0.01, lowpass=500, highpass=200)
        sparks = self.g.crackle(1.0, 30, (2000, 8000), within=(0.2, 0.4))
        self.assertLess(np.abs(sparks[:int(0.19 * self.rate)]).max(), 1e-3)
        self.assertGreater(np.abs(sparks[int(0.2 * self.rate):int(0.42 * self.rate)]).max(), 0.05)
        self.assertLess(np.abs(sparks[int(0.5 * self.rate):]).max(), 0.01)
        self.assertEqual(np.abs(self.g.crackle(0.5, 0, (500, 900))).max(), 0.0)
        late = self.g.crackle(0.05, 20, (500, 5000), within=lambda: 0.049, length=(0.02, 0.03))
        self.assertEqual(len(late), round(0.05 * self.rate))

    def test_thump_falls_in_pitch_and_pluck_holds_its_pitch(self):
        hit = self.g.thump(0.5, 200.0, 50.0, sweep=0.02, decay=0.3)
        early = dominant(hit[:2048], self.rate)
        late = dominant(hit[8000:16192], self.rate)
        self.assertGreater(early, late)
        string = self.g.pluck(220.0, 1.0, decay=0.8)
        body = string[2000:20000]
        lag = max(range(100, 300), key=lambda shift: float(np.dot(body[:-shift], body[shift:])))
        self.assertAlmostEqual(self.rate / lag, 220.0, delta=2)
        self.assertLess(np.abs(string[-2000:]).max(), np.abs(string[:2000]).max() * 0.05)

    def test_scatter_crops_grains_at_the_end(self):
        grains = self.g.scatter(0.01, 1000.0, np.ones(2000))
        self.assertEqual(len(grains), 441)
        self.assertGreater(grains.max(), 0)
        gain = np.zeros(441)
        self.assertEqual(self.g.scatter(0.01, 1000.0, np.ones(10), gain=gain).max(), 0)


class SeedTests(unittest.TestCase):
    def test_named_contexts_are_order_independent_and_reproducible(self):
        root = SoundGenerator(8000, seed=7)
        first = root.for_sound("Hit").noise(0.1)
        root.for_sound("Other").noise(1.0)
        np.testing.assert_array_equal(root.for_sound("Hit").noise(0.1), first)
        np.testing.assert_array_equal(SoundGenerator(8000, seed=7).for_sound("Hit").noise(0.1), first)
        self.assertFalse(np.array_equal(root.for_sound("Hit", variation=1).noise(0.1), first))
        self.assertFalse(np.array_equal(SoundGenerator(8000, seed=8).for_sound("Hit").noise(0.1), first))
        layer = root.for_sound("Hit").for_layer("air")
        self.assertEqual([scope["name"] for scope in layer.seed_info["scopes"]], ["Hit", "air"])
        np.testing.assert_array_equal(SoundGenerator(8000, seed=layer.seed).noise(0.1), layer.noise(0.1))

    def test_unseeded_contexts_record_their_seed(self):
        context = SoundGenerator(8000, seed=None)
        self.assertIsInstance(context.seed_info["seed"], int)
        np.testing.assert_array_equal(SoundGenerator(8000, seed=context.seed).noise(0.1), context.noise(0.1))
        with self.assertRaises(ValueError):
            SoundGenerator(8000, seed=-1)


if __name__ == "__main__":
    unittest.main()
