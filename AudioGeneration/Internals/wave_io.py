"""Audio file writing (WAV, FLAC, OGG) and source loading (WAV via SciPy, other formats via soundfile)."""

import io
import math
from pathlib import Path
import wave

import numpy as np

from .validation import mono


FORMATS = (".wav", ".ogg", ".flac")


def pcm(samples, rate, bits=16, dither=None):
    """Mono PCM WAV bytes (16 or 24 bit), rounded and clipped; `dither` is an RNG for TPDF dither."""
    if bits not in (16, 24):
        raise ValueError("bits must be 16 or 24")
    scale = float(2 ** (bits - 1) - 1)
    values = np.clip(mono(samples), -1.0, 1.0) * scale
    if dither is not None:
        # Triangular noise of +/- 1 LSB decorrelates quantization error from quiet tails.
        values += dither.uniform(-0.5, 0.5, len(values)) + dither.uniform(-0.5, 0.5, len(values))
    integers = np.clip(np.rint(values), -scale, scale).astype("<i4")
    if bits == 16:
        data = integers.astype("<i2").tobytes()
    else:
        data = integers.view(np.uint8).reshape(-1, 4)[:, :3].tobytes()
    output = io.BytesIO()
    with wave.open(output, "wb") as audio:
        audio.setnchannels(1)
        audio.setsampwidth(bits // 8)
        audio.setframerate(int(rate))
        audio.writeframes(data)
    return output.getvalue()


def save(path, samples, rate, bits, dither, quality):
    """Write by suffix: .wav (PCM 16/24), .flac (lossless 16/24) or .ogg (Vorbis, quality 0-1)."""
    path = Path(path)
    suffix = path.suffix.lower()
    if suffix not in FORMATS:
        raise ValueError(f"Unsupported audio file type {path.suffix!r}; use {', '.join(FORMATS)}")
    if not 0.0 <= float(quality) <= 1.0:
        raise ValueError("quality must be between 0 and 1")
    samples = mono(samples)
    path.parent.mkdir(parents=True, exist_ok=True)
    if suffix == ".wav":
        path.write_bytes(pcm(samples, rate, bits, dither))
        return path
    try:
        import soundfile
    except ImportError as error:
        raise ImportError(f"Writing {suffix} files needs soundfile (pip install soundfile)") from error
    if suffix == ".flac":
        if bits not in (16, 24):
            raise ValueError("bits must be 16 or 24")
        soundfile.write(path, np.clip(samples, -1.0, 1.0), int(rate), format="FLAC", subtype=f"PCM_{bits}")
    else:
        soundfile.write(path, np.clip(samples, -1.0, 1.0), int(rate), format="OGG", subtype="VORBIS",
                        compression_level=1.0 - float(quality))
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
