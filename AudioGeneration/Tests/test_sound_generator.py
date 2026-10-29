"""Public audio authoring contracts; no game assets or compiler required."""

from array import array
import contextlib
import io
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


class SoundGeneratorTests(unittest.TestCase):
    def test_scalar_path_runs_without_site_packages(self):
        code = """
import sys
from pathlib import Path
import tempfile
import wave
sys.path.insert(0, sys.argv[1])
from AudioGeneration import SoundGenerator
g = SoundGenerator()
samples = g.lowpass_samples([g.saw_sample(0.2, 3), 0.0, 0.0], 1000)
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / 'scalar.wav'
    g.write_wav(path, samples, rounding=True)
    with wave.open(str(path)) as audio:
        assert audio.getnframes() == len(samples)
assert 'numpy' not in sys.modules and 'scipy' not in sys.modules
"""
        subprocess.run([sys.executable, "-B", "-S", "-c", code, str(ENGINE_ROOT)],
                       check=True, capture_output=True, text=True)

    def test_numpy_only_path_does_not_load_scipy_or_soundfile(self):
        code = """
import sys
import numpy as np
sys.path.insert(0, sys.argv[1])
from AudioGeneration import SoundGenerator
g = SoundGenerator(seed=7, rounding='truncate')
t = g.time_axis(0.01)
samples = g.one_pole_bandpass(g.noise(0.01) + g.saw(55, t), 100, 3000)
samples = g.saturate(samples, 1.2)
assert np.isfinite(samples).all()
assert 'scipy' not in sys.modules and 'soundfile' not in sys.modules
"""
        subprocess.run([sys.executable, "-B", "-c", code, str(ENGINE_ROOT)],
                       check=True, capture_output=True, text=True)

    def test_wav_export_preserves_rate_length_and_clips(self):
        samples = np.array([-2.0, -0.5, 0.0, 0.5, 2.0])
        with tempfile.TemporaryDirectory() as directory:
            for rate in (44100, 48000):
                g = SoundGenerator(rate)
                path = Path(directory) / str(rate) / "test.wav"
                g.write_wav(path, samples)
                with wave.open(str(path)) as audio:
                    self.assertEqual((audio.getframerate(), audio.getnchannels(), audio.getsampwidth()),
                                     (rate, 1, 2))
                    self.assertEqual(audio.getnframes(), len(samples))
                    pcm = array("h", audio.readframes(audio.getnframes()))
                self.assertLess(pcm[0], pcm[1])
                self.assertEqual(pcm[2], 0)
                self.assertGreater(pcm[-1], pcm[-2])
                self.assertLessEqual(max(abs(value) for value in pcm), 32767)

    def test_layering_crops_at_buffer_end_and_segment_pads(self):
        g = SoundGenerator(10)
        target = np.zeros(10)
        result = g.mix_at(target, np.ones(5), 0.8, 0.5)
        self.assertIs(result, target)
        np.testing.assert_array_equal(target[:8], 0)
        np.testing.assert_array_equal(target[8:], 0.5)
        np.testing.assert_array_equal(g.segment(target, 0.5, 0.8), [0.5, 0.5, 0, 0, 0])
        before = target.copy()
        g.mix_at(target, np.ones(3), 1.5)
        np.testing.assert_array_equal(target, before)

    def test_lowpass_uses_hz_at_both_sample_rates(self):
        for rate in (44100, 48000):
            g = SoundGenerator(rate)
            t = g.time_axis(0.2)
            low = g.lowpass(np.sin(2 * np.pi * 100 * t), 500)
            high = g.lowpass(np.sin(2 * np.pi * 5000 * t), 500)
            tail = slice(len(t) // 2, None)
            self.assertGreater(np.std(low[tail]), 10 * np.std(high[tail]))

    def test_selection_rejects_unknown_names_before_writing(self):
        written = []
        sounds = {"Tap": None, "Hum": None}
        with contextlib.redirect_stdout(io.StringIO()) as output:
            SoundGenerator.run(sounds, written.append, prefix="Cue_", names=["Hum", "Tap"])
        self.assertEqual(written, ["Hum", "Tap"])
        self.assertEqual(output.getvalue().splitlines(), ["Cue_Hum.wav", "Cue_Tap.wav"])
        with self.assertRaises(SystemExit):
            SoundGenerator.run(sounds, written.append, names=["Tap", "Missing"])
        self.assertEqual(written, ["Hum", "Tap"])

    def test_seed_context_and_copy_fade(self):
        first = SoundGenerator(seed=23)
        second = SoundGenerator(seed=23)
        np.testing.assert_array_equal(first.noise(0.02), second.noise(0.02))
        samples = np.ones(100)
        faded = SoundGenerator(1000).fade(samples, 0.01, 0.02, copy=True)
        np.testing.assert_array_equal(samples, 1)
        self.assertEqual(faded[0], 0)
        self.assertEqual(faded[-1], 0)
        self.assertEqual(faded[50], 1)


if __name__ == "__main__":
    unittest.main()
