"""Files, sound packs, galleries and analysis reports through the public API."""

import contextlib
import io
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest
import wave

import numpy as np
from scipy.io import wavfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))

from AudioGeneration import SoundGenerator


def quiet(function, *args):
    with contextlib.redirect_stdout(io.StringIO()) as output:
        result = function(*args)
    return result, output.getvalue()


class FileTests(unittest.TestCase):
    def setUp(self):
        self.g = SoundGenerator(8000, seed=1)

    def test_wav_round_trip_rounds_clips_and_rejects_invalid_samples(self):
        with tempfile.TemporaryDirectory() as directory:
            path = self.g.write_wav(Path(directory) / "nested" / "a.wav", np.array([0.5, 1.5, -1.5, 0.25]))
            with wave.open(str(path)) as audio:
                self.assertEqual((audio.getnchannels(), audio.getsampwidth(), audio.getframerate()), (1, 2, 8000))
                frames = np.frombuffer(audio.readframes(4), "<i2")
            np.testing.assert_array_equal(frames, [16384, 32767, -32767, 8192])
            np.testing.assert_allclose(self.g.load(path), frames / 32768, atol=1e-9)
            with self.assertRaises(ValueError):
                self.g.write_wav(Path(directory) / "bad.wav", np.array([np.nan]))

    def test_load_downmixes_and_resamples(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "stereo.wav"
            wavfile.write(path, 16000, np.tile(np.array([0.2, 0.4], dtype=np.float32), (1600, 1)))
            samples = self.g.load(path)
            self.assertEqual(len(samples), 800)
            self.assertAlmostEqual(np.mean(samples[30:-30]), 0.3, delta=0.001)


class PackTests(unittest.TestCase):
    def make(self, directory):
        pack = SoundGenerator(8000, seed=9).pack(directory, prefix="Test_", peak=0.5)

        @pack.sound("Hit")
        def hit(g):
            return g.thump(0.3, 200, 60, click=0.2) + g.noise(0.3) * 0.05

        @pack.sound("Loud", lufs=-12, trim=None)
        def loud(g):
            return g.noise(0.2)

        @pack.sound("Elsewhere", output="other")
        def elsewhere(g):
            return g.sine(440, 0.1)

        return pack

    def test_each_sound_is_seeded_by_name_and_finished(self):
        with tempfile.TemporaryDirectory() as directory:
            pack = self.make(directory)
            hit = pack.render("Hit")
            np.testing.assert_array_equal(self.make(directory).render("Hit"), hit)
            self.assertAlmostEqual(np.abs(hit).max(), 0.5)
            self.assertEqual(hit[-1], 0.0)
            loud = pack.render("Loud")
            self.assertEqual(len(loud), 1600)
            self.assertAlmostEqual(SoundGenerator(8000).loudness(loud), -12, delta=0.01)
            self.assertEqual(pack.names, ["Hit", "Loud", "Elsewhere"])
            with self.assertRaises(ValueError):
                pack.sound("Hit")
            with self.assertRaises(ValueError):
                pack.sound("Typo", peek=0.5)

    def test_command_line_writes_selected_sounds_checks_and_galleries(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            pack = self.make(root / "out")
            rendered, output = quiet(pack.main, ["Hit", "Elsewhere"])
            self.assertEqual(sorted(rendered), ["Test_Elsewhere.wav", "Test_Hit.wav"])
            self.assertTrue((root / "out" / "Test_Hit.wav").exists())
            self.assertTrue((root / "out" / "other" / "Test_Elsewhere.wav").exists())
            self.assertFalse((root / "out" / "Test_Loud.wav").exists())
            self.assertIn("LUFS", output)
            quiet(pack.main, ["--check", "--output", str(root / "checked"), "--gallery", str(root / "g.html")])
            self.assertFalse((root / "checked").exists())
            self.assertTrue((root / "g.html").exists())
            with self.assertRaises(SystemExit), contextlib.redirect_stderr(io.StringIO()):
                pack.main(["Missing"])


class InspectionTests(unittest.TestCase):
    def setUp(self):
        self.g = SoundGenerator(8000, seed=2)

    def test_analysis_report_is_strict_json_with_loudness_and_warnings(self):
        signal = np.r_[np.ones(10) * 1.5, self.g.sine(500, 0.1)]
        report = self.g.analyze(signal)
        json.dumps(report, allow_nan=False)
        self.assertEqual(report["clipping_sample_count"], 10)
        self.assertIsNotNone(report["loudness_lufs"])
        self.assertIn("nonzero_start", [warning["code"] for warning in report["warnings"]])
        self.assertIn("LUFS", self.g.format_report(report))
        silent = self.g.analyze(np.zeros(10))
        self.assertIsNone(silent["loudness_lufs"])
        invalid = self.g.analyze(np.array([np.nan, 0.0]))
        self.assertEqual(invalid["invalid_samples"]["nan"], 1)
        with tempfile.TemporaryDirectory() as directory:
            paths = self.g.write_report(Path(directory) / "report", report)
            self.assertEqual(json.loads(paths["json"].read_text(encoding="utf-8"))["kind"], "audio")

    def test_gallery_embeds_escaped_data_and_true_level_audio(self):
        label = "Hit </script><script>alert(1)</script>"
        with tempfile.TemporaryDirectory() as directory:
            path = self.g.export_gallery(Path(directory) / "page.html", {label: self.g.sine(300, 0.2) * 0.25},
                                         title="Set")
            html = path.read_text(encoding="utf-8")
            self.assertNotIn(label, html)
            self.assertNotIn("fetch(", html)
            payload = re.search(r'<script id="gallery-data" type="application/json">(.*?)</script>', html, re.S)
            sound = json.loads(payload.group(1))["sounds"][0]
            self.assertEqual(sound["name"], label)
            self.assertAlmostEqual(sound["peak_dbfs"], 20 * np.log10(0.25), places=2)
            self.assertTrue(sound["audio"].startswith("data:audio/wav;base64,"))
            with self.assertRaises(ValueError):
                self.g.export_gallery(Path(directory) / "empty.html", {})


if __name__ == "__main__":
    unittest.main()
