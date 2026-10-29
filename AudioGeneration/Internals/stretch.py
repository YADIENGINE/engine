"""Time stretching (WSOLA) and pitch shifting that keeps the duration."""

import math

import numpy as np

from .validation import mono, positive

FRAME_SECONDS = 0.04
SEARCH_SECONDS = 0.012


def stretch(samples, factor, rate):
    """Waveform-similarity overlap-add: factor 2 doubles the duration without changing pitch."""
    samples = mono(samples)
    factor = positive(factor, "factor")
    if factor == 1.0 or len(samples) < 2:
        return samples
    frame = max(16, 2 * round(FRAME_SECONDS * rate / 2))
    hop = frame // 2
    search = max(1, round(SEARCH_SECONDS * rate))
    window = np.hanning(frame + 1)[:-1]  # periodic Hann: 50% overlaps sum to one
    tail = 2 * frame + search + math.ceil(2 * hop / factor)
    padded = np.concatenate([np.zeros(frame + search), samples, np.zeros(tail)])
    origin = frame + search
    length = round(len(samples) * factor)
    frames = math.ceil(length / hop) + 2
    output = np.zeros(frames * hop + frame)
    # Frame k is centered on input k * hop / factor and on output k * hop + hop.
    start = origin - hop
    previous = start
    for index in range(frames):
        nominal = start + round(index * hop / factor)
        if index:
            # Pick the analysis frame most similar to the natural continuation of the previous one.
            target = padded[previous + hop:previous + hop + frame]
            low = max(0, nominal - search)
            region = padded[low:nominal + search + frame]
            scores = np.correlate(region, target, mode="valid")
            energy = np.sqrt(np.convolve(region ** 2, np.ones(frame), mode="valid")) + 1e-12
            nominal = low + int(np.argmax(scores / energy))
        output[index * hop:index * hop + frame] += padded[nominal:nominal + frame] * window
        previous = nominal
    # Drop the first half frame (no overlap partner); output time zero is then input time zero.
    return output[hop:hop + length]


def pitch_shift(samples, semitones, rate):
    """Shift pitch by `semitones` keeping the duration (stretch, then resample)."""
    from .effects import speed

    samples = mono(samples)
    ratio = 2.0 ** (float(semitones) / 12.0)
    if not math.isfinite(ratio):
        raise ValueError("semitones must be finite")
    if ratio == 1.0 or not len(samples):
        return samples
    shifted = speed(stretch(samples, ratio, rate), ratio)
    result = np.zeros(len(samples))
    result[:min(len(samples), len(shifted))] = shifted[:len(samples)]
    return result
