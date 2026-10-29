"""Trapezoidal state-variable filter with sample-wise log-frequency smoothing.

Mathematical topology: Andrew Simper, Cytomic, SvfLinearTrapOptimised2.pdf.
See Docs/AudioGeneration.md for the derivation reference and supported contract.
"""

import math

import numpy as np

from .validation import finite_mono, nonnegative, positive


def smooth_filter(samples, cutoff, kind, q, smoothing_seconds, sample_rate):
    samples = finite_mono(samples)
    sample_rate = positive(sample_rate, "sample_rate")
    q = positive(q, "q")
    smoothing_seconds = nonnegative(smoothing_seconds, "smoothing_seconds")
    if kind not in ("lowpass", "highpass", "bandpass"):
        raise ValueError("kind must be lowpass, highpass or bandpass")
    if np.iscomplexobj(cutoff):
        raise ValueError("cutoff must contain real Hz values")
    cutoff = np.asarray(cutoff, dtype=float)
    if cutoff.ndim > 1 or (cutoff.ndim == 1 and len(cutoff) != len(samples)):
        raise ValueError("cutoff must be scalar Hz or one Hz value per sample")
    if not np.isfinite(cutoff).all() or np.any(cutoff <= 0) or np.any(cutoff >= sample_rate * 0.49):
        raise ValueError("cutoff must be positive and below 0.49 * sample_rate")
    if not len(samples):
        return samples.copy()
    targets = np.broadcast_to(np.log(cutoff), samples.shape)
    smoothing = -math.expm1(-1.0 / (smoothing_seconds * sample_rate)) if smoothing_seconds else 1.0
    current = float(targets[0])
    state_band = state_low = 0.0
    damping = 1.0 / q
    output = np.empty_like(samples)
    for index, sample in enumerate(samples):
        current += smoothing * (targets[index] - current)
        conductance = math.tan(math.pi * math.exp(current) / sample_rate)
        inverse = 1.0 / (1.0 + conductance * (conductance + damping))
        coupled = conductance * inverse
        residual = sample - state_low
        band = inverse * state_band + coupled * residual
        low = state_low + coupled * state_band + conductance * coupled * residual
        state_band = 2.0 * band - state_band
        state_low = 2.0 * low - state_low
        if kind == "lowpass":
            output[index] = low
        elif kind == "highpass":
            output[index] = sample - damping * band - low
        else:
            output[index] = band
    if not np.isfinite(output).all():
        raise ValueError("Filter output overflowed; reduce input level, Q or modulation speed")
    return output
