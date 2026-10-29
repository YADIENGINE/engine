"""Reference comparison and portable listening export through the public API."""

import json
from pathlib import Path
import re
import sys
import tempfile
import unittest
import wave

import numpy as np
from scipy.io import wavfile

ENGINE_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ENGINE_ROOT))

from AudioGeneration import SoundGenerator


class ReferenceReviewTests(unittest.TestCase):
    def setUp(self):
        self.g = SoundGenerator(8000, seed=42)
        t = self.g.time_axis(0.12)
        self.signal = 0.4 * np.sin(2 * np.pi * 500 * t) * np.minimum(t / 0.01, 1) * np.exp(-t / 0.06)

    def test_alignment_ignores_padding_but_reports_original_levels(self):
        reference = np.r_[np.zeros(160), self.signal, np.zeros(80)]
        candidate = np.r_[np.zeros(1600), self.signal * 0.5, np.zeros(800)]
        result = self.g.compare(reference, candidate)
        self.assertAlmostEqual(result["alignment"]["candidate_offset_seconds"] -
                               result["alignment"]["reference_offset_seconds"], 0.18)
        metrics = result["metrics"]["all"]
        self.assertAlmostEqual(metrics["duration_seconds"]["delta"], 0)
        self.assertAlmostEqual(metrics["rms_dbfs"]["delta"], 20 * np.log10(0.5), places=6)
        self.assertAlmostEqual(metrics["rise_10_90_seconds"]["delta"], 0)
        self.assertGreater(result["candidate"]["raw"]["duration_seconds"], result["reference"]["raw"]["duration_seconds"])
        self.assertEqual(self.g.compare(reference, candidate, align=False)["alignment"]["candidate_offset_seconds"], 0)

    def test_empty_and_silent_stages_are_unavailable_not_perfect_matches(self):
        for source in (np.array([]), np.zeros(100)):
            result = self.g.compare(source, self.signal, focus="tail")
            self.assertIsNone(result["reference"]["landmarks"]["onset_seconds"])
            self.assertIsNone(result["metrics"]["tail"]["rms_dbfs"]["delta"])
            json.dumps(result, allow_nan=False)
            self.assertIn("Reference comparison", self.g.format_report(result))
        with tempfile.TemporaryDirectory() as directory:
            paths = self.g.export_review(directory, [], {"Sound": self.signal})
            with wave.open(str(paths["audio"]["reference"]["preview"])) as audio:
                self.assertEqual(audio.getnframes(), 1)
            data = json.loads(paths["report"]["json"].read_text(encoding="utf-8"))
            self.assertEqual(data["clips"]["reference"]["features"]["raw"]["sample_count"], 0)

    def test_review_clip_snapshots_parameters_layers_and_seed(self):
        context = self.g.for_sound("Impact", variation=2)
        parameters = {"decay": 0.1, "notes": ["dry"]}
        samples = self.signal.copy()
        clip = context.review_clip(layers={"body": context.for_layer("body").layer(samples, at=0.02)}, parameters=parameters)
        samples[:] = 0
        parameters["notes"].append("changed")
        self.assertFalse(clip.samples.flags.writeable)
        self.assertGreater(np.max(np.abs(clip.samples)), 0)
        self.assertEqual(clip.parameters["notes"], ["dry"])
        self.assertEqual(clip.layer_metadata["body"]["random"]["scopes"][-1]["name"], "body")

    def test_wav_loading_preserves_dc_downmixes_and_resamples_without_touching_source(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "source.wav"
            wavfile.write(path, 16000, np.tile(np.array([0.2, 0.4], dtype=np.float32), (1600, 1)))
            original = path.read_bytes()
            clip = self.g.review_clip(path)
            self.assertEqual(len(clip.samples), 800)
            self.assertAlmostEqual(np.mean(clip.samples[30:-30]), 0.3, delta=0.001)
            self.assertEqual(clip.source["original_sample_rate"], 16000)
            self.assertIsNone(clip.random["seed"])
            self.assertEqual(path.read_bytes(), original)

    def test_portable_export_contains_reports_raw_audio_and_exact_browser_seeds(self):
        context = self.g.for_sound("Impact")
        candidate = context.review_clip(layers={"body": context.for_layer("body").layer(self.signal * 5),
                                                "air": context.for_layer("air").layer(self.signal * 0.1)},
                                        parameters={"drive": 5})
        label = "Variant </script><script>not-code</script>"
        with tempfile.TemporaryDirectory() as directory:
            paths = self.g.export_review(directory, self.signal, {label: candidate})
            report = json.loads(paths["report"]["json"].read_text(encoding="utf-8"))
            html = paths["html"].read_text(encoding="utf-8")
            embedded = re.search(r'<script id="review-data" type="application/json">(.*?)</script>', html, re.S).group(1)
            data = json.loads(embedded)
            variant = data["clips"][1]
            self.assertEqual(variant["name"], label)
            self.assertEqual(variant["features"]["random"]["seed"], str(context.seed_info["seed"]))
            self.assertNotIn(label, html)
            self.assertTrue(variant["tracks"]["variant-001"]["data_uri"].startswith("data:audio/wav;base64,"))
            self.assertNotIn("fetch(", html)
            self.assertEqual(report["kind"], "review")
            self.assertEqual(len(variant["tracks"]), 3)
            stem = variant["tracks"]["variant-001-layer-001"]
            self.assertAlmostEqual(stem["analysis"]["peak_amplitude"], np.max(np.abs(candidate.layers["body"])))
            self.assertEqual(stem["analysis"]["random"]["seed"],
                             str(candidate.layer_metadata["body"]["random"]["seed"]))
            rate, raw = wavfile.read(paths["audio"]["variant-001"]["raw"])
            self.assertEqual(rate, 8000)
            self.assertGreater(np.max(np.abs(raw)), 1)
            np.testing.assert_allclose(raw, candidate.samples, atol=1e-7)
            for entry in paths["audio"].values():
                with wave.open(str(entry["preview"])) as audio:
                    self.assertEqual(audio.getframerate(), 8000)
            self.assertIn("Variant", paths["report"]["text"].read_text(encoding="utf-8"))

    def test_matching_uses_active_rms_and_preserves_layer_balance(self):
        clip = self.g.review_clip(layers={"body": self.g.layer(self.signal),
                                         "air": self.g.layer(self.signal * 0.25)})
        with tempfile.TemporaryDirectory() as directory:
            paths = self.g.export_review(directory, np.r_[np.zeros(2000), self.signal], {"Quiet": clip}, target_dbfs=-30)
            data = json.loads(paths["report"]["json"].read_text(encoding="utf-8"))
            for item in data["clips"].values():
                self.assertAlmostEqual(item["matching"]["rms"]["result_level_dbfs"], -30)
            variant = data["clips"]["variant-001"]
            self.assertEqual(len(variant["tracks"]), 3)
            self.assertLessEqual(variant["matching"]["rms"]["gain"] *
                                 max(track["peak"] for track in variant["tracks"].values()), 0.89 + 1e-12)

    def test_bad_inputs_fail_before_export_and_source_collision_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "session"
            with self.assertRaises(ValueError):
                self.g.export_review(destination, self.signal, {"bad": np.array([np.nan])})
            self.assertFalse(destination.exists())
            with self.assertRaises(ValueError):
                self.g.review_clip(self.signal, parameters={"bad": float("nan")})
            with self.assertRaises(ValueError):
                self.g.compare(self.signal, SoundGenerator(16000).review_clip([0]))
            source = destination / "raw" / "reference.wav"
            source.parent.mkdir(parents=True)
            wavfile.write(source, 8000, self.signal.astype(np.float32))
            original = source.read_bytes()
            with self.assertRaises(ValueError):
                self.g.export_review(destination, source, {"test": self.signal})
            self.assertEqual(source.read_bytes(), original)
            self.assertFalse((destination / "index.html").exists())

    def test_matching_reports_peak_and_boost_limits_instead_of_clipping(self):
        spike = np.zeros(1000)
        spike[200] = 0.9
        with tempfile.TemporaryDirectory() as directory:
            paths = self.g.export_review(directory, self.signal,
                                         {"Transient": spike, "Quiet": self.signal * 0.01},
                                         target_dbfs=-3, max_gain_db=3)
            data = json.loads(paths["report"]["json"].read_text(encoding="utf-8"))
            transient = data["clips"]["variant-001"]["matching"]["rms"]
            quiet = data["clips"]["variant-002"]["matching"]["rms"]
            self.assertTrue(transient["limited_by_peak"])
            self.assertLessEqual(transient["gain"] * 0.9, 0.89 + 1e-12)
            self.assertTrue(quiet["limited_by_gain"])
            self.assertLess(quiet["result_level_dbfs"], -3)


if __name__ == "__main__":
    unittest.main()
