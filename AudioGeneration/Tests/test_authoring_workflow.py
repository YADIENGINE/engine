"""Analysis, named stems and order-independent RNG through the public API."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import wave

import numpy as np

ENGINE_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ENGINE_ROOT))

from AudioGeneration import SoundGenerator


class AuthoringWorkflowTests(unittest.TestCase):
    def test_analysis_measures_tone_and_silence_without_mutating(self):
        g = SoundGenerator(8000)
        tone = 0.5 * np.sin(2 * np.pi * 1000 * g.time_axis(0.1))
        samples = np.concatenate([np.zeros(80), tone, np.zeros(160)])
        before = samples.copy()
        report = g.analyze(samples)
        self.assertAlmostEqual(report["duration_seconds"], 0.13)
        self.assertAlmostEqual(report["peak_amplitude"], 0.5)
        self.assertAlmostEqual(report["rms_amplitude"], np.sqrt(np.mean(samples ** 2)))
        self.assertGreaterEqual(report["leading_silence_seconds"], 0.01)
        self.assertGreaterEqual(report["trailing_silence_seconds"], 0.02)
        self.assertAlmostEqual(report["spectral"]["centroid_hz"], 1000, delta=50)
        self.assertGreater(report["spectral"]["band_energy_ratio"]["mid"], 0.95)
        self.assertGreater(len(report["frames"]), 1)
        self.assertAlmostEqual(sum(report["spectral"]["band_energy_ratio"].values()), 1)
        np.testing.assert_array_equal(samples, before)

    def test_analysis_handles_invalid_empty_silent_and_short_signals(self):
        g = SoundGenerator(8000)
        report = g.analyze([0, 2, np.nan, np.inf, -np.inf])
        self.assertEqual(report["clipping_sample_count"], 1)
        self.assertEqual(report["invalid_samples"], {"nan": 1, "positive_inf": 1, "negative_inf": 1})
        self.assertIsNone(report["rms_dbfs"])
        self.assertIn("non_finite", [warning["code"] for warning in report["warnings"]])
        for samples in ([], [0], np.zeros(50), [0.1], [1e308, -1e308]):
            result = g.analyze(samples)
            json.dumps(result, allow_nan=False)
            self.assertTrue(g.format_report(result))
        with self.assertRaises(ValueError):
            g.analyze(np.zeros((2, 10)))
        with self.assertRaises(ValueError):
            g.analyze([0], frame_seconds=0)

    def test_named_seeds_ignore_sibling_order_and_preserve_legacy_stream(self):
        g = SoundGenerator(seed=72)
        sound = g.for_sound("Impact", variation=2)
        expected = sound.for_layer("body").noise(0.01)
        g.for_sound("Other").noise(0.4)
        sound.for_layer("air").noise(0.3)
        np.testing.assert_array_equal(expected, g.for_sound("Impact", variation=2).for_layer("body").noise(0.01))
        np.testing.assert_array_equal(g.noise(0.01), SoundGenerator(seed=72).noise(0.01))
        self.assertFalse(np.array_equal(expected, g.for_sound("Impact", variation=3).for_layer("body").noise(0.01)))
        restored = SoundGenerator(seed=sound.seed_info["seed"])
        np.testing.assert_array_equal(expected, restored.for_layer("body").noise(0.01))
        self.assertEqual(sound.seed_info["scopes"][0]["name"], "Impact")

    def test_scoped_seed_is_independent_of_process_hash_seed(self):
        code = """
import sys
sys.path.insert(0, sys.argv[1])
from AudioGeneration import SoundGenerator
print(SoundGenerator(seed=3).for_sound('Whoosh').for_layer('air').seed_info['seed'])
"""
        results = []
        for hash_seed in ("1", "42"):
            result = subprocess.run([sys.executable, "-B", "-S", "-c", code, str(ENGINE_ROOT)],
                                    env=dict(os.environ, PYTHONHASHSEED=hash_seed),
                                    check=True, capture_output=True, text=True)
            results.append(result.stdout)
        self.assertEqual(results[0], results[1])

    def test_named_layers_snapshot_align_gain_and_crop(self):
        g = SoundGenerator(10, seed=7)
        source = np.ones(3)
        body = g.for_layer("body").layer(source, at=0.2, gain=0.5)
        source[:] = 99
        self.assertFalse(body.samples.flags.writeable)
        layers = {"body": body, "air": g.layer([0.2, 0.2], at=0.4)}
        np.testing.assert_allclose(g.mix_layers(layers), [0, 0, 0.5, 0.5, 0.7, 0.2])
        np.testing.assert_allclose(g.mix_layers(layers, seconds=0.4), [0, 0, 0.5, 0.5])
        report = g.analyze_layers(layers, seconds=0.4)
        self.assertEqual(report["layers"]["body"]["retained_sample_count"], 2)
        self.assertEqual(report["layers"]["air"]["retained_sample_count"], 0)
        self.assertEqual(report["layers"]["body"]["analysis"]["random"], body.random)
        with self.assertRaises(ValueError):
            g.layer([0], at=-0.1)
        with self.assertRaises(ValueError):
            g.mix_layers({"wrong_rate": SoundGenerator(20).layer([0])})

    def test_layer_export_writes_aligned_stems_and_raw_measurements(self):
        g = SoundGenerator(8000, seed=42).for_sound("Impact")
        layers = {"body": g.for_layer("body").layer(np.full(8, 0.8)),
                  "air": g.for_layer("air").layer(np.full(4, 0.8), at=4 / 8000)}
        with tempfile.TemporaryDirectory() as directory:
            paths = g.export_layers(directory, layers)
            report = json.loads(paths["report"]["json"].read_text(encoding="utf-8"))
            self.assertAlmostEqual(report["mix"]["peak_amplitude"], 1.6)
            self.assertEqual(report["mix"]["clipping_sample_count"], 4)
            self.assertIn("Layer: body", paths["report"]["text"].read_text(encoding="utf-8"))
            for path in [paths["mix"], *paths["layers"].values()]:
                with wave.open(str(path)) as audio:
                    self.assertEqual(audio.getnframes(), 8)
                    self.assertEqual(audio.getframerate(), 8000)
            with wave.open(str(paths["layers"]["air"])) as audio:
                samples = np.frombuffer(audio.readframes(8), dtype="<i2")
            np.testing.assert_array_equal(samples[:4], 0)
            self.assertTrue(np.all(samples[4:] > 0))

    def test_invalid_layers_can_be_inspected_but_export_creates_no_files(self):
        g = SoundGenerator()
        layers = {"invalid": g.layer([0, np.nan])}
        report = g.analyze_layers(layers)
        self.assertEqual(report["layers"]["invalid"]["analysis"]["invalid_samples"]["nan"], 1)
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(ValueError):
                g.export_layers(directory, layers)
            self.assertEqual(list(Path(directory).iterdir()), [])


if __name__ == "__main__":
    unittest.main()
