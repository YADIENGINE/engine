"""Standard-library algorithms for authoring without NumPy or SciPy."""

import math


def saw(phase, harmonics):
    return sum(math.sin(n * phase) / n for n in range(1, harmonics + 1))


def lowpass(samples, cutoff, sample_rate):
    alpha = 1.0 - math.exp(-2.0 * math.pi * cutoff / sample_rate)
    out, state = [], 0.0
    for sample in samples:
        state += alpha * (sample - state)
        out.append(state)
    return out
