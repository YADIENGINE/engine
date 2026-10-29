"""Mono PCM16 export and optional source decoding."""

from array import array
from pathlib import Path
import wave


def write_wav(path, samples, sample_rate, rounding, clip):
    if rounding:
        data = array("h", (round(max(-1.0, min(1.0, s)) * 32767) for s in samples))
    else:
        import numpy as np

        data = ((np.clip(samples, -1.0, 1.0) if clip else samples) * 32767.0).astype("<i2")
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        output.writeframes(data.tobytes())


def read_source(path, sample_rate):
    import numpy as np
    from scipy.signal import resample_poly
    import soundfile as sf

    audio, rate = sf.read(path, always_2d=True)
    audio = audio.mean(axis=1)
    if rate != sample_rate:
        divisor = np.gcd(rate, sample_rate)
        audio = resample_poly(audio, sample_rate // divisor, rate // divisor)
    return audio - audio.mean()
