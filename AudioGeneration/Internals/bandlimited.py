"""Nyquist-limited additive oscillators, with tapered moving harmonic cutoffs."""

import math

import numpy as np

from .validation import positive, positive_integer


def oscillator(frequency, count, sample_rate, phase, max_harmonics, square):
    sample_rate = positive(sample_rate, "sample_rate")
    max_harmonics = positive_integer(max_harmonics, "max_harmonics")
    phase = float(phase)
    if not math.isfinite(phase):
        raise ValueError("phase must be finite radians")
    phase %= 2.0 * math.pi
    if np.iscomplexobj(frequency):
        raise ValueError("frequency must contain real Hz values")
    frequency = np.asarray(frequency, dtype=float)
    scalar = frequency.ndim == 0
    if not scalar and frequency.ndim != 1:
        raise ValueError("frequency must be scalar Hz or a one-dimensional Hz curve")
    if not np.isfinite(frequency).all() or np.any(frequency <= 0) or np.any(frequency >= sample_rate / 2):
        raise ValueError("frequency must be positive and below Nyquist")
    if not scalar:
        if count is not None and count != len(frequency):
            raise ValueError("seconds does not match the frequency curve's sample count")
        count = len(frequency)
    elif count is None:
        raise ValueError("seconds is required for scalar frequency")
    if count < 0:
        raise ValueError("Duration must not be negative")
    if not count:
        return np.zeros(0)
    # Phase is defined at each sample time, beginning exactly at the supplied phase.
    if scalar:
        angle = phase + 2.0 * np.pi * float(frequency) * np.arange(count) / sample_rate
    else:
        angle = np.empty(count)
        angle[0] = phase
        angle[1:] = phase + 2.0 * np.pi * np.cumsum(frequency[:-1]) / sample_rate
    nyquist = sample_rate / 2.0
    upper = max(1, int(min(max_harmonics, nyquist * 0.95 / float(np.min(frequency)))))
    output = np.zeros(count)
    for harmonic in range(1, upper + 1, 2 if square else 1):
        # Fade a partial out over 80%-95% of Nyquist, avoiding abrupt harmonic switches.
        progress = np.clip((frequency * harmonic / nyquist - 0.80) / 0.15, 0.0, 1.0)
        weight = 1.0 if harmonic == 1 else 0.5 + 0.5 * np.cos(np.pi * progress)
        output += weight * np.sin(harmonic * angle) / harmonic
    return output * ((4.0 if square else -2.0) / np.pi)
