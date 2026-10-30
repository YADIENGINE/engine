"""Control curves: envelopes, pitch contours, LFOs and smooth randomness (one value per sample)."""

import math

import numpy as np

from .validation import choice, curve, finite, mono, nonnegative, positive, unit


def time_axis(count, rate):
    return np.arange(count) / rate


def _points(points, name):
    try:
        pairs = [(finite(at, f"{name} time"), finite(value, f"{name} value")) for at, value in points]
    except (TypeError, ValueError) as error:
        raise ValueError(f"{name} needs (seconds, value) pairs with finite numbers") from error
    if not pairs:
        raise ValueError(f"{name} needs at least one point")
    times = np.array([at for at, _ in pairs])
    if times[0] < 0 or np.any(np.diff(times) < 0):
        raise ValueError(f"{name} times must start at or after zero and never decrease")
    return times, np.array([value for _, value in pairs])


def envelope(points, count, rate, shape):
    """Piecewise segments; shape < 1 moves fast first, > 1 slow first. Holds the end values."""
    shape = positive(shape, "curve")
    times, values = _points(points, "envelope")
    t = time_axis(count, rate)
    if len(times) == 1:
        return np.full(count, values[0])
    index = np.clip(np.searchsorted(times, t, side="right") - 1, 0, len(times) - 2)
    start, end = times[index], times[index + 1]
    span = end - start
    with np.errstate(divide="ignore", invalid="ignore"):
        progress = np.where(span > 0, np.clip((t - start) / np.where(span > 0, span, 1.0), 0.0, 1.0), 1.0)
    progress = np.where(t < times[0], 0.0, progress)
    return values[index] + (values[index + 1] - values[index]) * progress ** shape


def contour(points, count, rate):
    """Geometric (log-domain) interpolation for Hz, ratios and other positive values."""
    times, values = _points(points, "contour")
    if np.any(values <= 0):
        raise ValueError("contour values must be positive")
    return np.exp(np.interp(time_axis(count, rate), times, np.log(values)))


def adsr(count, rate, attack, decay, sustain, release, shape):
    seconds = count / rate
    attack, decay, release = (nonnegative(v, n) for v, n in ((attack, "attack"), (decay, "decay"),
                                                               (release, "release")))
    sustain = unit(sustain, "sustain")
    # Shorten the stages proportionally when they do not fit the duration.
    total = attack + decay + release
    if total > seconds and total > 0:
        scale = seconds / total
        attack, decay, release = attack * scale, decay * scale, release * scale
    release_at = seconds - release
    points = [(0.0, 0.0 if attack else 1.0), (attack, 1.0), (attack + decay, sustain), (release_at, sustain),
              (seconds, 0.0 if release else sustain)]
    return envelope(points, count, rate, shape)


def decay(count, rate, time_constant, attack, hold):
    """Linear rise over `attack`, full level for `hold`, then exp(-t / time_constant)."""
    time_constant = positive(time_constant, "time_constant")
    attack, hold = nonnegative(attack, "attack"), nonnegative(hold, "hold")
    t = time_axis(count, rate)
    rise = np.minimum(t / attack, 1.0) if attack else 1.0
    return rise * np.exp(-np.maximum(t - attack - hold, 0.0) / time_constant)


def rise(count, rate, seconds, shape):
    """0 to 1 over `seconds`, then 1; shape < 1 rises fast first, > 1 slow first."""
    seconds = nonnegative(seconds, "time")
    if not seconds:
        return np.ones(count)
    return np.minimum(time_axis(count, rate) / seconds, 1.0) ** positive(shape, "curve")


def glide(count, rate, start, end, time_constant):
    start, end = finite(start, "start"), finite(end, "end")
    time_constant = positive(time_constant, "time_constant")
    return end + (start - end) * np.exp(-time_axis(count, rate) / time_constant)


def lfo(count, rate, frequency, shape, phase, center, depth):
    choice(shape, "shape", ("sine", "triangle", "square", "saw"))
    frequency = curve(frequency, count, "rate")
    cycles = finite(phase, "phase") / (2 * math.pi) + np.concatenate([[0.0], np.cumsum(frequency[:-1])]) / rate
    cycles %= 1.0
    if shape == "sine":
        wave = np.sin(2 * math.pi * cycles)
    elif shape == "triangle":
        wave = 1.0 - 4.0 * np.abs(((cycles + 0.25) % 1.0) - 0.5)
    elif shape == "square":
        wave = np.where(cycles < 0.5, 1.0, -1.0)
    else:
        wave = 2.0 * cycles - 1.0
    return finite(center, "center") + finite(depth, "depth") * wave


def random_curve(rng, count, rate, frequency, low, high):
    frequency = positive(frequency, "rate")
    low, high = finite(low, "low"), finite(high, "high")
    keys = rng.uniform(low, high, max(2, math.ceil(count / rate * frequency) + 2))
    position = time_axis(count, rate) * frequency
    index = np.minimum(position.astype(int), len(keys) - 2)
    # Cosine interpolation: smooth, no overshoot beyond [low, high].
    blend = (1.0 - np.cos(np.pi * (position - index))) / 2.0
    return keys[index] + (keys[index + 1] - keys[index]) * blend


def fade(samples, fade_in, fade_out, shape):
    result = mono(samples)
    shape = positive(shape, "curve")
    for count, ramp in ((min(fade_in, len(result)), slice(None)), (min(fade_out, len(result)), slice(None, None, -1))):
        if count:
            window = np.ones(len(result))
            window[:count] = np.linspace(0.0, 1.0, count, endpoint=False) ** shape
            result *= window[ramp]
    return result
