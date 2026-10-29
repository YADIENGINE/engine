"""PCM16 WAV encoding/writing and source loading (WAV via SciPy, other formats via soundfile)."""

import io
import math
from pathlib import Path
import wave

import numpy as np

from .validation import mono


def pcm16(samples, rate):
    """Rounded, clipped mono PCM16 WAV bytes."""
    data = np.rint(np.clip(mono(samples), -1.0, 1.0) * 32767.0).astype("<i2")
    output = io.BytesIO()
    with wave.open(output, "wb") as audio:
        audio.setnchannels(1)
        audio.setsampwidth(2)
        audio.setframerate(int(rate))
        audio.writeframes(data.tobytes())
    return output.getvalue()


def write_wav(path, samples, rate):
    payload = pcm16(samples, rate)
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    return path


def load(path, target_rate):
    """Decode, downmix by channel mean and resample to the context rate. DC and level are untouched."""
    path = Path(path)
    if path.suffix.lower() == ".wav":
        from scipy.io import wavfile

        rate, audio = wavfile.read(path)
        if audio.dtype.kind == "u":
            audio = (audio.astype(float) - 128.0) / 128.0
        elif audio.dtype.kind == "i":
            audio = audio.astype(float) / float(2 ** (audio.dtype.itemsize * 8 - 1))
        else:
            audio = audio.astype(float)
    else:
        try:
            import soundfile
        except ImportError as error:
            raise ImportError(f"Reading {path.suffix} files needs soundfile (pip install soundfile)") from error
        audio, rate = soundfile.read(path, always_2d=True)
    if audio.ndim == 2:
        audio = audio.mean(axis=1)
    audio = mono(audio, path.name)
    if rate != target_rate and len(audio):
        from scipy.signal import resample_poly

        divisor = math.gcd(int(rate), int(target_rate))
        audio = resample_poly(audio, int(target_rate) // divisor, int(rate) // divisor)
    return audio, {"filename": path.name, "original_sample_rate": int(rate)}
