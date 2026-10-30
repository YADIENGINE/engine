"""Dynamics: compressor, look-ahead limiter, gate and transient shaper (all offline, peak-envelope based)."""

import math

import numpy as np

from .effects import follow
from .validation import finite, mono, nonnegative, positive

FLOOR = 1e-9


def _db(values):
    return 20.0 * np.log10(np.maximum(values, FLOOR))


def compress(samples, threshold_db, ratio, attack, release, knee_db, makeup_db, rate):
    """Downward compressor with a soft knee; gain follows the peak envelope (no look-ahead)."""
    samples = mono(samples)
    threshold_db, makeup_db = finite(threshold_db, "threshold_db"), finite(makeup_db, "makeup_db")
    ratio, knee_db = positive(ratio, "ratio"), nonnegative(knee_db, "knee_db")
    if ratio < 1:
        raise ValueError("ratio must be at least 1")
    level = _db(follow(samples, attack, release, rate))
    over = level - threshold_db
    slope = 1.0 - 1.0 / ratio
    if knee_db:
        knee = np.clip(over + knee_db / 2.0, 0.0, knee_db)
        reduction = np.where(over > knee_db / 2.0, slope * over, slope * knee ** 2 / (2.0 * knee_db))
    else:
        reduction = slope * np.maximum(over, 0.0)
    return samples * 10.0 ** ((makeup_db - reduction) / 20.0)


def limit(samples, ceiling, lookahead, release, rate):
    """Look-ahead brickwall limiter: the sample peak never exceeds `ceiling`; gain recovers over `release`."""
    from scipy.ndimage import minimum_filter1d, uniform_filter1d

    samples = mono(samples)
    ceiling = positive(ceiling, "ceiling")
    if not len(samples):
        return samples
    span = max(1, round(nonnegative(lookahead, "lookahead") * rate))
    needed = np.minimum(1.0, ceiling / np.maximum(np.abs(samples), FLOOR))
    # Each averaged value only mixes minima that already cover this sample, so the ceiling holds.
    gain = uniform_filter1d(minimum_filter1d(needed, 2 * span + 1, mode="nearest"), span, mode="nearest")
    gain = np.minimum(gain, needed)
    recover = 1.0 - math.exp(-1.0 / (positive(release, "release") * rate))
    smoothed = [0.0] * len(gain)
    level = 1.0
    for index, target in enumerate(gain.tolist()):
        level = target if target < level else level + recover * (target - level)
        smoothed[index] = level
    return samples * np.minimum(np.array(smoothed), needed)


def gate(samples, threshold_db, attack, release, floor_db, rate):
    """Attenuate to `floor_db` while the envelope is below the threshold; gain moves smoothly."""
    samples = mono(samples)
    threshold = 10.0 ** (finite(threshold_db, "threshold_db") / 20.0)
    floor = 10.0 ** (finite(floor_db, "floor_db") / 20.0)
    # A fast detector lets the gain smoothing, not the detector, set how quickly the gate closes.
    level = follow(samples, 0.0005, 0.01, rate)
    target = np.where(level >= threshold, 1.0, floor)
    rise = 1.0 - math.exp(-1.0 / (positive(attack, "attack") * rate))
    fall = 1.0 - math.exp(-1.0 / (positive(release, "release") * rate))
    gain = [0.0] * len(target)
    current = floor
    for index, value in enumerate(target.tolist()):
        current += (rise if value > current else fall) * (value - current)
        gain[index] = current
    return samples * np.array(gain)


def shape_transients(samples, attack_db, sustain_db, rate):
    """Boost/cut onsets and sustain separately: a fast envelope above a slow one marks a transient."""
    samples = mono(samples)
    attack_db, sustain_db = finite(attack_db, "attack_db"), finite(sustain_db, "sustain_db")
    fast = follow(samples, 0.0005, 0.05, rate)
    slow = follow(samples, 0.02, 0.05, rate)
    onset = np.clip((fast - slow) / np.maximum(fast, FLOOR), 0.0, 1.0)
    return samples * 10.0 ** ((attack_db * onset + sustain_db * (1.0 - onset)) / 20.0)
