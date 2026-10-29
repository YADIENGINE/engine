"""Seeded bursts and resonant pings used to layer synthetic textures."""

import numpy as np

from .filters import butterworth


def crackles(rng, seconds, per_second, decay, color_low, color_high, gain_curve, sample_rate):
    count = int(seconds * sample_rate)
    out = np.zeros(count)
    grain = int(0.03 * sample_rate)
    shape = np.exp(-np.arange(grain) / (decay * sample_rate))
    for _ in range(int(per_second * seconds)):
        at = rng.integers(0, max(count - grain, 1))
        amplitude = rng.uniform(0.3, 1.0)
        if gain_curve is not None:
            amplitude *= gain_curve[at]
        out[at:at + grain] += rng.uniform(-1, 1, grain) * shape * amplitude
    return butterworth(out, [color_low, color_high], "bandpass", 2, sample_rate)


def pings(rng, seconds, per_second, low, high, decay, gain_curve, sample_rate):
    count = int(seconds * sample_rate)
    out = np.zeros(count)
    length = int(decay * 6 * sample_rate)
    t = np.arange(length) / sample_rate
    for _ in range(int(per_second * seconds)):
        at = rng.integers(0, max(count - length, 1))
        frequency = rng.uniform(low, high)
        amplitude = rng.uniform(0.2, 1.0) * (gain_curve[at] if gain_curve is not None else 1.0)
        tone = (np.sin(2 * np.pi * frequency * t)
                + 0.4 * np.sin(2 * np.pi * frequency * 2.76 * t)) * np.exp(-t / decay)
        out[at:at + length] += tone * amplitude
    return out
