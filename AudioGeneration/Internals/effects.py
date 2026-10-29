"""Convolution room and saturation, including opt-in filtered oversampling."""

import numpy as np

from .filters import butterworth


def reverb(samples, rng, count, seconds, mix, damp, predelay_count, sample_rate):
    from scipy import signal

    t = np.arange(count) / sample_rate
    impulse = rng.normal(0.0, 1.0, len(t)) * np.exp(-t * 6.9 / seconds)
    impulse = butterworth(impulse, damp, "lowpass", 2, sample_rate)
    impulse = np.concatenate([np.zeros(predelay_count), impulse])
    impulse /= np.sqrt(np.sum(impulse ** 2)) + 1e-9
    wet = signal.fftconvolve(samples, impulse)[:len(samples) + len(impulse) - 1]
    dry = np.concatenate([samples, np.zeros(len(wet) - len(samples))])
    wet *= np.max(np.abs(dry)) / (np.max(np.abs(wet)) + 1e-9)
    return dry * (1.0 - mix) + wet * mix


def saturate(samples, drive):
    return np.tanh(samples * drive) / np.tanh(drive)


def saturate_oversampled(samples, drive, factor):
    from scipy import signal

    from .validation import finite_mono, nonnegative, positive_integer

    samples = finite_mono(samples)
    drive = nonnegative(drive, "drive")
    factor = positive_integer(factor, "oversample")
    if factor not in (2, 4, 8, 16):
        raise ValueError("oversample must be 1, 2, 4, 8 or 16")
    if not len(samples) or not drive:
        return samples.copy()
    if len(samples) == 1:
        return saturate(samples, drive)
    # Symmetric FIR at the elevated rate; retain a transition band before Nyquist.
    half_span = 64
    kernel = signal.firwin(2 * half_span * factor + 1, 0.94 / factor, window=("kaiser", 8.6))
    padding = half_span * 2
    padded = np.pad(samples, (padding, padding), mode="edge")
    elevated = signal.resample_poly(padded, factor, 1, window=kernel)
    distorted = saturate(elevated, drive)
    restored = signal.resample_poly(distorted, 1, factor, window=kernel)
    result = restored[padding:padding + len(samples)].copy()
    if not np.isfinite(result).all():
        raise ValueError("Distortion output overflowed; reduce input level or drive")
    return result
