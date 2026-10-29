"""Whole-signal trailing-energy detection, including late reflections."""

import math

import numpy as np

from .validation import finite_mono, nonnegative, positive


def trim_energy(samples, sample_rate, threshold_dbfs, window_seconds, hold_seconds,
                fade_seconds, min_seconds, peak_guard_db):
    samples = finite_mono(samples)
    sample_rate = positive(sample_rate, "sample_rate")
    window_seconds = positive(window_seconds, "window_seconds")
    hold_seconds = nonnegative(hold_seconds, "hold_seconds")
    fade_seconds = nonnegative(fade_seconds, "fade_seconds")
    min_seconds = nonnegative(min_seconds, "min_seconds")
    peak_guard_db = nonnegative(peak_guard_db, "peak_guard_db")
    threshold_dbfs = float(threshold_dbfs)
    if not math.isfinite(threshold_dbfs) or threshold_dbfs > 0:
        raise ValueError("threshold_dbfs must be finite and at or below zero")
    if not len(samples):
        return samples.copy()
    minimum = min(len(samples), round(min_seconds * sample_rate))
    peak = float(np.max(np.abs(samples)))
    last_active_end = 0
    if peak:
        window = max(1, round(window_seconds * sample_rate))
        starts = np.arange(0, len(samples), window)
        lengths = np.minimum(window, len(samples) - starts)
        scaled = samples / peak
        rms = np.sqrt(np.add.reduceat(scaled ** 2, starts) / lengths)
        frame_peaks = np.maximum.reduceat(np.abs(scaled), starts)
        # Compare logarithms to keep absolute dBFS thresholds safe for very quiet input.
        with np.errstate(divide="ignore"):
            rms_dbfs = 20.0 * (np.log10(rms) + math.log10(peak))
            peak_dbfs = 20.0 * (np.log10(frame_peaks) + math.log10(peak))
        active = np.flatnonzero((rms_dbfs > threshold_dbfs) | (peak_dbfs > threshold_dbfs + peak_guard_db))
        if len(active):
            last = int(active[-1])
            last_active_end = int(starts[last] + lengths[last])
    hold = round(hold_seconds * sample_rate)
    fade = round(fade_seconds * sample_rate)
    end = min(len(samples), max(minimum, last_active_end + hold + fade)) if last_active_end else minimum
    result = samples[:end].copy()
    fade = min(fade, len(result))
    if fade:
        result[-fade:] *= np.linspace(1.0, 0.0, fade)
    return result
