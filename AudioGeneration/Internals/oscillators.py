"""Existing noise and oscillator implementations; no automatic anti-aliasing changes."""

import numpy as np


def brown(rng, count):
    from scipy import signal

    white = rng.normal(0.0, 1.0, count)
    out = signal.lfilter([1.0], [1.0, -0.995], white)
    return out / (np.max(np.abs(out)) + 1e-9)


def smooth_random(rng, count, seconds, rate, low, high):
    points = max(int(seconds * rate) + 3, 4)
    keys = rng.uniform(low, high, points)
    x = np.linspace(0, points - 1, count)
    return np.interp(x, np.arange(points), keys)


def saw_phase(phase, harmonics):
    out = np.zeros_like(phase)
    for harmonic in range(1, harmonics + 1):
        out += np.sin(phase * harmonic) / harmonic
    return out * (2.0 / np.pi)


def saw(frequency, t, harmonics, sample_rate):
    out = np.zeros_like(t)
    for harmonic in range(1, harmonics + 1):
        if frequency * harmonic > sample_rate / 2:
            break
        out += np.sin(2.0 * np.pi * frequency * harmonic * t) / harmonic
    return out * (2.0 / np.pi)


def pulse(t, frequency, width):
    phase = (t * frequency) % 1.0
    return np.tanh(6.0 * (np.where(phase < width, 1.0, -1.0) * 0.5
                         + 0.5 * np.sin(2.0 * np.pi * frequency * t)))


def sawtooth(phase):
    from scipy import signal

    return signal.sawtooth(phase)


def square(phase):
    from scipy import signal

    return signal.square(phase)
