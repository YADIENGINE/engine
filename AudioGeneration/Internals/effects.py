"""Waveshaping, bit crushing, echo, chorus, convolution room, playback speed and envelope following.

Wet/dry effects share one rule: output = dry * (1 - mix) + wet * mix.
"""

from fractions import Fraction
import math

import numpy as np

from .filters import butterworth
from .validation import choice, finite, mono, nonnegative, positive, positive_integer, unit

OVERSAMPLING = (1, 2, 4, 8, 16)


SHAPES = ("tanh", "clip", "fold")


def _curve(shape, drive, bias):
    """Transfer function with silence kept at zero, scaled so inputs in [-1, 1] stay in [-1, 1]."""
    if shape == "fold":
        # Sine wavefolder: drive past 1 folds peaks back instead of flattening them.
        function = lambda x: np.sin(0.5 * math.pi * x)
    elif shape == "tanh":
        function = np.tanh
    else:
        function = lambda x: np.clip(x, -1.0, 1.0)
    offset = float(function(drive * bias))
    probe = function(drive * (np.linspace(-1.0, 1.0, 2001) + bias)) - offset
    scale = float(np.max(np.abs(probe))) or 1.0
    return lambda x: (function(drive * (x + bias)) - offset) / scale


def oversampled(samples, function, factor):
    """Apply a nonlinearity at `factor` times the rate (Kaiser FIR), keeping length and alignment."""
    from scipy import signal

    if factor not in OVERSAMPLING:
        raise ValueError("oversample must be 1, 2, 4, 8 or 16")
    if factor == 1 or len(samples) < 2:
        return function(samples)
    # Symmetric Kaiser FIR, transition band below the output Nyquist; edges padded then removed.
    half_span = 64
    kernel = signal.firwin(2 * half_span * factor + 1, 0.94 / factor, window=("kaiser", 8.6))
    padding = half_span * 2
    padded = np.pad(samples, (padding, padding), mode="edge")
    elevated = signal.resample_poly(padded, factor, 1, window=kernel)
    restored = signal.resample_poly(function(elevated), 1, factor, window=kernel)
    return restored[padding:padding + len(samples)].copy()


def saturate(samples, drive, shape, bias, oversample):
    """Waveshaper: soft (tanh), hard (clip) or folding; `bias` makes it asymmetric (even harmonics).

    Symmetric tanh/clip map input 1 to 1; every curve keeps inputs in [-1, 1] within [-1, 1].
    """
    samples = mono(samples)
    drive = nonnegative(drive, "drive")
    choice(shape, "shape", SHAPES)
    bias = finite(bias, "bias")
    if not len(samples) or not drive:
        return samples
    return oversampled(samples, _curve(shape, drive, bias), oversample)


def bitcrush(samples, bits, hold_rate, rate):
    samples = mono(samples)
    if bits is not None:
        levels = 2.0 ** (positive_integer(bits, "bits") - 1)
        samples = np.round(samples * levels) / levels
    if hold_rate is not None:
        step = rate / positive(hold_rate, "rate")
        if step > 1:
            held = (np.floor(np.arange(len(samples)) / step) * step).astype(int)
            samples = samples[held]
    return samples


def _wet_dry(dry, wet, mix):
    mix = unit(mix, "mix")
    length = max(len(dry), len(wet))
    return np.pad(dry, (0, length - len(dry))) * (1.0 - mix) + np.pad(wet, (0, length - len(wet))) * mix


def delay(samples, time, feedback, mix, damp, rate):
    """Feedback echo; each repeat is `feedback` quieter (and darker with `damp`). Adds the tail."""
    samples = mono(samples)
    offset = round(positive(time, "time") * rate)
    feedback = unit(feedback, "feedback", closed=False)
    if damp is not None:
        positive(damp, "damp")
    repeats = 1 if not feedback else min(256, 1 + math.ceil(math.log(1e-3) / math.log(feedback)))
    wet = np.zeros(len(samples) + offset * repeats)
    echo = samples
    for index in range(1, repeats + 1):
        if damp is not None:
            echo = butterworth(echo, damp, "lowpass", 1, rate)
        wet[index * offset:index * offset + len(echo)] += echo
        echo = echo * feedback
    return _wet_dry(samples, wet, mix)


def chorus(samples, rate_hz, depth, base_delay, mix, rate):
    samples = mono(samples)
    rate_hz, depth, base_delay = positive(rate_hz, "rate"), nonnegative(depth, "depth"), positive(base_delay, "delay")
    if depth >= base_delay:
        raise ValueError("depth must be smaller than delay")
    t = np.arange(len(samples)) / rate
    wet = np.zeros_like(samples)
    for voice, offset in enumerate((0.0, math.pi * 2 / 3)):
        lag = (base_delay + depth * np.sin(2 * math.pi * rate_hz * (1.0 + 0.13 * voice) * t + offset)) * rate
        wet += np.interp(np.arange(len(samples)) - lag, np.arange(len(samples)), samples, left=0.0) * 0.5
    return _wet_dry(samples, wet, mix)


# Octave bands for the room: (low edge, high edge) Hz; None is an open end.
ROOM_BANDS = ((None, 177.0), (177.0, 354.0), (354.0, 707.0), (707.0, 1414.0), (1414.0, 2828.0),
              (2828.0, 5657.0), (5657.0, 11314.0), (11314.0, None))


def room_impulse(rng, seconds, damp, rate):
    """Noise impulse whose bands decay separately: RT60 = seconds / (1 + (band center / damp)^2)."""
    count = round(seconds * rate)
    t = np.arange(count) / rate
    noise = rng.normal(0.0, 1.0, count)
    impulse = np.zeros(count)
    for low, high in ROOM_BANDS:
        if low is not None and low >= rate * 0.45:
            break
        high = None if high is not None and high >= rate * 0.45 else high
        if low is None:
            band, center = butterworth(noise, high, "lowpass", 4, rate), high / 2 ** 0.5
        elif high is None:
            band, center = butterworth(noise, low, "highpass", 4, rate), low * 2 ** 0.5
        else:
            band, center = butterworth(noise, (low, high), "bandpass", 2, rate), (low * high) ** 0.5
        decay = seconds / (1.0 + (center / damp) ** 2)
        impulse += band * np.exp(-t * 6.9 / decay)
    return impulse


def reverb(rng, samples, seconds, mix, damp, predelay, impulse, rate):
    """Unit-energy convolution room (synthetic, or a supplied impulse response). Adds the tail."""
    from scipy import signal

    samples = mono(samples)
    if impulse is None:
        impulse = room_impulse(rng, positive(seconds, "seconds"), positive(damp, "damp"), rate)
    else:
        impulse = mono(impulse, "impulse")
    impulse = np.concatenate([np.zeros(round(nonnegative(predelay, "predelay") * rate)), impulse])
    energy = math.sqrt(float(np.sum(impulse ** 2)))
    if not energy:
        raise ValueError("impulse response is silent")
    wet = signal.fftconvolve(samples, impulse / energy) if len(samples) else samples
    return _wet_dry(samples, wet, mix)


def speed(samples, factor):
    """Tape-style playback: factor 2 is an octave up at half the duration."""
    from scipy import signal

    samples = mono(samples)
    ratio = Fraction(positive(factor, "factor")).limit_denominator(1000)
    if ratio == 1 or not len(samples):
        return samples
    return signal.resample_poly(samples, ratio.denominator, ratio.numerator)


def follow(samples, attack, release, rate):
    """Peak envelope follower (one-pole attack/release on |x|), for ducking and gating."""
    samples = mono(samples)
    rise = 1.0 - math.exp(-1.0 / (positive(attack, "attack") * rate))
    fall = 1.0 - math.exp(-1.0 / (positive(release, "release") * rate))
    level = 0.0
    output = [0.0] * len(samples)
    for index, value in enumerate(np.abs(samples).tolist()):
        level += (rise if value > level else fall) * (value - level)
        output[index] = level
    return np.array(output)

