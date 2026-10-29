"""Convolution room and soft saturation extracted from existing authoring code."""

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
