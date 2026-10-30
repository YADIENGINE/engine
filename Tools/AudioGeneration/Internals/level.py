"""Level: peak and loudness normalization, energy-aware tail trimming and the finishing chain."""

import math

import numpy as np

from .validation import finite, mono, nonnegative, positive

GATE_SECONDS = 0.4


def _k_weighting(rate):
    """ITU-R BS.1770 pre-filter (high shelf + high-pass) re-derived for any rate, as in libebur128."""
    k = math.tan(math.pi * 1681.974450955533 / rate)
    q = 0.7071752369554196
    vh = 10 ** (3.999843853973347 / 20)
    vb = vh ** 0.4996667741545416
    a0 = 1 + k / q + k * k
    shelf = [(vh + vb * k / q + k * k) / a0, 2 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0,
             1.0, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0]
    k = math.tan(math.pi * 38.13547087602444 / rate)
    q = 0.5003270373238773
    a0 = 1 + k / q + k * k
    highpass = [1.0, -2.0, 1.0, 1.0, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0]
    return np.array([shelf, highpass])


def loudness(samples, rate):
    """Integrated loudness in LUFS (mono BS.1770 with gating); clips under 400 ms are measured whole."""
    from scipy import signal

    samples = mono(samples)
    if not len(samples) or not np.any(samples):
        return None
    weighted = signal.sosfilt(_k_weighting(rate), samples)
    block = round(GATE_SECONDS * rate)
    if len(weighted) < block:
        power = float(np.mean(weighted ** 2))
        return -0.691 + 10 * math.log10(power) if power > 0 else None
    hop = max(1, block // 4)
    energy = np.concatenate([[0.0], np.cumsum(weighted ** 2)])
    starts = np.arange(0, len(weighted) - block + 1, hop)
    powers = (energy[starts + block] - energy[starts]) / block
    with np.errstate(divide="ignore"):
        levels = -0.691 + 10 * np.log10(powers)
    gated = powers[levels > -70.0]
    if not len(gated):
        return None
    relative = -0.691 + 10 * math.log10(float(np.mean(gated))) - 10.0
    gated = powers[(levels > -70.0) & (levels > relative)]
    return -0.691 + 10 * math.log10(float(np.mean(gated)))


def normalize(samples, peak, lufs, rate):
    samples = mono(samples)
    if (peak is None) == (lufs is None):
        raise ValueError("normalize needs exactly one of peak or lufs")
    if peak is not None:
        current = float(np.max(np.abs(samples))) if len(samples) else 0.0
        return samples * (positive(peak, "peak") / current) if current else samples
    measured = loudness(samples, rate)
    return samples * 10 ** ((finite(lufs, "lufs") - measured) / 20) if measured is not None else samples


def trim(samples, rate, threshold_dbfs, window, hold, fade, minimum):
    """Cut after the last window whose RMS (or peak - 12 dB) is above the threshold, then fade out."""
    samples = mono(samples)
    threshold_dbfs = finite(threshold_dbfs, "threshold_dbfs")
    window, hold, fade, minimum = (positive(window, "window"), nonnegative(hold, "hold"),
                                   nonnegative(fade, "fade"), nonnegative(minimum, "minimum"))
    if not len(samples):
        return samples
    size = max(1, round(window * rate))
    starts = np.arange(0, len(samples), size)
    lengths = np.minimum(size, len(samples) - starts)
    rms = np.sqrt(np.add.reduceat(samples ** 2, starts) / lengths)
    peaks = np.maximum.reduceat(np.abs(samples), starts)
    threshold = 10 ** (threshold_dbfs / 20)
    active = np.flatnonzero((rms > threshold) | (peaks > threshold * 10 ** (12 / 20)))
    last = int(starts[active[-1]] + lengths[active[-1]]) if len(active) else 0
    fade_count = round(fade * rate)
    end = min(len(samples), max(round(minimum * rate), last + round(hold * rate) + fade_count if last else 0))
    result = samples[:end].copy()
    fade_count = min(fade_count, len(result))
    if fade_count:
        result[-fade_count:] *= np.linspace(1.0, 0.0, fade_count)
    return result


def finish(samples, rate, peak, lufs, ceiling, dc, fade_in, fade_out, trim_dbfs):
    """DC block -> level -> tail trim -> edge fades -> exact level -> peak ceiling. Rejects NaN/Inf.

    The first level pass makes the trim threshold refer to the exported level; the second makes the
    target exact after trimming and fading changed the signal.
    """
    from .curves import fade
    from .filters import dc_block

    samples = mono(samples)
    ceiling = positive(ceiling, "ceiling")
    if ceiling > 1:
        raise ValueError("ceiling must not exceed 1")
    if dc:
        samples = dc_block(samples, 20.0 if dc is True else dc, rate)
    target = peak is not None or lufs is not None
    if target:
        samples = normalize(samples, peak, lufs, rate)
    if trim_dbfs is not None:
        samples = trim(samples, rate, trim_dbfs, 0.01, 0.05, 0.0, 0.0)
    samples = fade(samples, round(nonnegative(fade_in, "fade_in") * rate),
                   round(nonnegative(fade_out, "fade_out") * rate), 1.0)
    if target:
        samples = normalize(samples, peak, lufs, rate)
    highest = float(np.max(np.abs(samples))) if len(samples) else 0.0
    return samples * (ceiling / highest) if highest > ceiling else samples
