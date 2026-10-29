"""Fixed Butterworth filters and per-sample state-variable filters for moving cutoffs.

The SVF topology follows Andrew Simper (Cytomic), "SvfLinearTrapOptimised2.pdf"; cutoff curves are
smoothed in log-Hz so sweeps stay click-free.
"""

import math

import numpy as np

from .validation import curve, finite, mono, positive, positive_integer

SMOOTHING_SECONDS = 0.005


def _fixed(value):
    return np.ndim(value) == 0


def butterworth(samples, cutoff, kind, order, rate):
    from scipy import signal

    order = positive_integer(order, "order")
    nyquist = rate / 2
    for value in np.atleast_1d(cutoff):
        if not 0 < float(value) < nyquist:
            raise ValueError("cutoff must be between 0 Hz and Nyquist")
    if not len(samples):
        return samples
    return signal.sosfilt(signal.butter(order, cutoff, kind, fs=rate, output="sos"), samples)


def _smoothed_log(cutoff, count, rate):
    from scipy import signal

    targets = np.log(cutoff)
    alpha = -math.expm1(-1.0 / (SMOOTHING_SECONDS * rate))
    smoothed, _ = signal.lfilter([alpha], [1.0, alpha - 1.0], targets, zi=[targets[0] * (1.0 - alpha)])
    return np.exp(smoothed)


def svf(samples, cutoff, kind, q, rate):
    """One two-pole stage; cutoff and q may vary per sample. Band output has unity peak gain."""
    count = len(samples)
    cutoff = curve(cutoff, count, "cutoff", low=0.0, high=rate * 0.49)
    q = curve(q, count, "q", low=0.0)
    if not count:
        return samples
    if not np.all(cutoff == cutoff[0]):
        cutoff = _smoothed_log(cutoff, count, rate)
    damping = 1.0 / q
    g = np.tan(np.pi * cutoff / rate)
    a1 = 1.0 / (1.0 + g * (g + damping))
    k = damping.tolist()
    a2 = g * a1
    a3 = g * a2
    output = [0.0] * count
    ic1 = ic2 = 0.0
    mode = ("lowpass", "highpass", "bandpass").index(kind)
    for index, (x, c1, c2, c3, d) in enumerate(zip(samples.tolist(), a1.tolist(), a2.tolist(), a3.tolist(), k)):
        v3 = x - ic2
        v1 = c1 * ic1 + c2 * v3
        v2 = ic2 + c2 * ic1 + c3 * v3
        ic1 = 2.0 * v1 - ic1
        ic2 = 2.0 * v2 - ic2
        output[index] = v2 if mode == 0 else (x - d * v1 - v2 if mode == 1 else d * v1)
    result = np.array(output)
    if not np.isfinite(result).all():
        raise ValueError("Filter output overflowed; reduce input level or Q")
    return result


def _butterworth_qs(order):
    if order % 2:
        raise ValueError("a moving cutoff needs an even order (2, 4, 6 ...)")
    return [1.0 / (2.0 * math.sin(math.pi * (2 * k - 1) / (2 * order))) for k in range(1, order // 2 + 1)]


def pass_filter(samples, cutoff, kind, order, rate):
    samples = mono(samples)
    if _fixed(cutoff):
        return butterworth(samples, float(cutoff), kind, order, rate)
    for q in _butterworth_qs(positive_integer(order, "order")):
        samples = svf(samples, cutoff, kind, q, rate)
    return samples


def bandpass(samples, low, high, order, rate, center=None, width=None):
    samples = mono(samples)
    if (center is None) != (width is None) or (center is not None and (low is not None or high is not None)):
        raise ValueError("bandpass takes low and high, or center and width")
    if center is not None:
        width = positive(width, "width")
        if width <= 1:
            raise ValueError("width is a ratio above 1 (2 = one octave each side)")
        low, high = np.divide(center, width), np.multiply(center, width)
    elif low is None or high is None:
        raise ValueError("bandpass takes low and high, or center and width")
    if _fixed(low) and _fixed(high):
        if not float(low) < float(high):
            raise ValueError("low must be below high")
        return butterworth(samples, (float(low), float(high)), "bandpass", order, rate)
    low = curve(low, len(samples), "low", low=0.0)
    high = curve(high, len(samples), "high", low=0.0)
    if np.any(low >= high):
        raise ValueError("low must stay below high")
    center = np.sqrt(low * high)
    q = center / (high - low)
    for _ in range(positive_integer(order, "order")):
        samples = svf(samples, center, "bandpass", q, rate)
    return samples


def resonate(samples, center, q, rate):
    return svf(mono(samples), center, "bandpass", q, rate)


def formants(samples, bands, rate):
    samples = mono(samples)
    output = np.zeros_like(samples)
    try:
        bands = [(center, q, float(gain)) for center, q, gain in bands]
    except (TypeError, ValueError) as error:
        raise ValueError("formants needs (center Hz or curve, q, gain) triples") from error
    for center, q, gain in bands:
        output += gain * svf(samples, center, "bandpass", q, rate)
    return output


def dc_block(samples, cutoff, rate):
    return butterworth(mono(samples), positive(cutoff, "cutoff"), "highpass", 1, rate)



def _rbj(kind, frequency, gain_db, q, rate):
    """Audio EQ Cookbook (Robert Bristow-Johnson) peaking and shelving biquads as one SOS row."""
    if not 0 < frequency < rate / 2:
        raise ValueError("frequency must be between 0 Hz and Nyquist")
    amplitude = 10.0 ** (gain_db / 40.0)
    w = 2.0 * math.pi * frequency / rate
    cos, alpha = math.cos(w), math.sin(w) / (2.0 * q)
    if kind == "bell":
        b = [1 + alpha * amplitude, -2 * cos, 1 - alpha * amplitude]
        a = [1 + alpha / amplitude, -2 * cos, 1 - alpha / amplitude]
    else:
        root = 2.0 * math.sqrt(amplitude) * alpha
        sign = 1.0 if kind == "low" else -1.0
        b = [amplitude * ((amplitude + 1) - sign * (amplitude - 1) * cos + root),
             sign * 2 * amplitude * ((amplitude - 1) - sign * (amplitude + 1) * cos),
             amplitude * ((amplitude + 1) - sign * (amplitude - 1) * cos - root)]
        a = [(amplitude + 1) + sign * (amplitude - 1) * cos + root,
             -sign * 2 * ((amplitude - 1) + sign * (amplitude + 1) * cos),
             (amplitude + 1) + sign * (amplitude - 1) * cos - root]
    return np.array([[*(value / a[0] for value in b), 1.0, a[1] / a[0], a[2] / a[0]]])


def equalize(samples, kind, frequency, gain_db, q, rate):
    from scipy import signal

    samples = mono(samples)
    sos = _rbj(kind, positive(frequency, "frequency"), finite(gain_db, "gain_db"), positive(q, "q"), rate)
    return signal.sosfilt(sos, samples) if len(samples) else samples


# Average adult formants (Hz) and bandwidths (Hz) for F1-F3; relative gains fall with frequency.
VOWELS = {"a": (730, 1090, 2440), "e": (530, 1840, 2480), "i": (270, 2290, 3010), "o": (570, 840, 2410),
          "u": (300, 870, 2240)}
VOWEL_BANDWIDTHS = (90.0, 110.0, 160.0)
VOWEL_GAINS = (1.0, 0.5, 0.25)


def vowel(samples, shape, scale, rate):
    """Formant filter for one vowel or [(seconds, vowel), ...] moving between vowels (log-Hz)."""
    samples = mono(samples)
    scale = positive(scale, "scale")
    if isinstance(shape, str):
        shape = [(0.0, shape)]
    try:
        points = [(float(at), VOWELS[name]) for at, name in shape]
    except (KeyError, TypeError, ValueError) as error:
        raise ValueError(f"vowel takes one of {', '.join(VOWELS)} or [(seconds, vowel), ...]") from error
    if not points:
        raise ValueError("vowel needs at least one point")
    t = np.arange(len(samples)) / rate
    times = np.array([at for at, _ in points])
    if times[0] < 0 or np.any(np.diff(times) < 0):
        raise ValueError("vowel times must start at or after zero and never decrease")
    bands = []
    for index, (bandwidth, gain) in enumerate(zip(VOWEL_BANDWIDTHS, VOWEL_GAINS)):
        center = np.exp(np.interp(t, times, np.log([hz[index] for _, hz in points]))) * scale
        center = np.minimum(center, rate * 0.45)
        bands.append((center, np.maximum(center / (bandwidth * scale), 0.5), gain))
    return formants(samples, bands, rate)
