"""Saturation, bit crushing, echo, chorus, convolution room, playback speed and envelope following.

Wet/dry effects share one rule: output = dry * (1 - mix) + wet * mix.
"""

from fractions import Fraction
import math

import numpy as np

from .filters import butterworth
from .validation import mono, nonnegative, positive, positive_integer, unit

OVERSAMPLING = (1, 2, 4, 8, 16)


def _tanh(samples, drive):
    return np.tanh(samples * drive) / math.tanh(drive)


def saturate(samples, drive, oversample):
    """Normalized tanh (input 1 maps to 1), run at an elevated rate to limit aliasing."""
    from scipy import signal

    samples = mono(samples)
    drive = nonnegative(drive, "drive")
    if oversample not in OVERSAMPLING:
        raise ValueError("oversample must be 1, 2, 4, 8 or 16")
    if not len(samples) or not drive:
        return samples
    if oversample == 1 or len(samples) == 1:
        return _tanh(samples, drive)
    # Symmetric Kaiser FIR, transition band below the output Nyquist; edges padded then removed.
    half_span = 64
    kernel = signal.firwin(2 * half_span * oversample + 1, 0.94 / oversample, window=("kaiser", 8.6))
    padding = half_span * 2
    padded = np.pad(samples, (padding, padding), mode="edge")
    elevated = signal.resample_poly(padded, oversample, 1, window=kernel)
    restored = signal.resample_poly(_tanh(elevated, drive), 1, oversample, window=kernel)
    return restored[padding:padding + len(samples)].copy()


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


def reverb(rng, samples, seconds, mix, damp, predelay, rate):
    """Unit-energy noise room: a steady input keeps its RMS in the wet signal. Adds the tail."""
    from scipy import signal

    samples = mono(samples)
    seconds, damp = positive(seconds, "seconds"), positive(damp, "damp")
    count = round(seconds * rate)
    t = np.arange(count) / rate
    impulse = rng.normal(0.0, 1.0, count) * np.exp(-t * 6.9 / seconds)
    impulse = butterworth(impulse, min(damp, rate * 0.45), "lowpass", 2, rate)
    impulse = np.concatenate([np.zeros(round(nonnegative(predelay, "predelay") * rate)), impulse])
    impulse /= np.sqrt(np.sum(impulse ** 2)) or 1.0
    wet = signal.fftconvolve(samples, impulse) if len(samples) else samples
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

