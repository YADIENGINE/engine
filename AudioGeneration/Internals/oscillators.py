"""Band-limited oscillators, FM, colored noise and physical sources (modal, thump, pluck)."""

import math
import operator

import numpy as np

from .curves import decay as decay_curve, glide
from .filters import bandpass, butterworth
from .validation import choice, curve, finite, frequency as hz, nonnegative, positive, positive_integer, unit

WAVES = ("saw", "square", "triangle")


def phase(frequency, count, rate, start):
    """Radians at each sample: begins at `start`, advances with the previous sample's Hz."""
    frequency, count = hz(frequency, count, rate)
    result = np.empty(count)
    if count:
        result[0] = 0.0
        result[1:] = np.cumsum(frequency[:-1])
    return finite(start, "phase") + 2.0 * math.pi * result / rate


def sine(frequency, count, rate, start):
    return np.sin(phase(frequency, count, rate, start))


def additive(frequency, count, rate, start, max_harmonics, wave):
    """Fourier series with partials faded out over 80-95% of Nyquist (no hard switches)."""
    choice(wave, "wave", WAVES)
    max_harmonics = positive_integer(max_harmonics, "max_harmonics")
    frequency, count = hz(frequency, count, rate)
    if not count:
        return np.zeros(0)
    angle = phase(frequency, count, rate, start)
    nyquist = rate / 2.0
    upper = max(1, int(min(max_harmonics, nyquist * 0.95 / float(np.min(frequency)))))
    constant = float(np.min(frequency)) == float(np.max(frequency))
    step = 1 if wave == "saw" else 2
    base = np.exp(1j * angle)
    stride = base ** step
    rotor = base.copy()
    output = np.zeros(count)
    for harmonic in range(1, upper + 1, step):
        ratio = (frequency[0] if constant else frequency) * harmonic / nyquist
        weight = 1.0 if harmonic == 1 else 0.5 + 0.5 * np.cos(np.pi * np.clip((ratio - 0.80) / 0.15, 0.0, 1.0))
        if wave == "triangle":
            amplitude = (-1.0) ** ((harmonic - 1) // 2) / harmonic ** 2
        else:
            amplitude = 1.0 / harmonic
        output += weight * amplitude * rotor.imag
        rotor *= stride
        if harmonic % 64 == 63:
            rotor /= np.abs(rotor)  # keep the recurrence on the unit circle
    scale = {"saw": -2.0 / math.pi, "square": 4.0 / math.pi, "triangle": 8.0 / math.pi ** 2}[wave]
    return output * scale


def pulse(frequency, count, rate, start, width, max_harmonics):
    width = unit(width, "width")
    if width in (0.0, 1.0):
        raise ValueError("width must be strictly between 0 and 1")
    lagging = additive(frequency, count, rate, start - 2.0 * math.pi * width, max_harmonics, "saw")
    return lagging - additive(frequency, count, rate, start, max_harmonics, "saw")


def unison(rng, frequency, count, rate, wave, voices, detune, max_harmonics):
    """Detuned stack (supersaw and friends): voices spread evenly over +/- `detune` cents, random phases."""
    choice(wave, "wave", ("sine", *WAVES))
    voices = positive_integer(voices, "voices")
    detune = nonnegative(detune, "detune")
    frequency, count = hz(frequency, count, rate)
    output = np.zeros(count)
    for cents in (np.linspace(-detune, detune, voices) if voices > 1 else [0.0]):
        shifted = frequency * 2.0 ** (cents / 1200.0)
        start = rng.uniform(0.0, 2.0 * math.pi)
        output += (sine(shifted, None, rate, start) if wave == "sine" else
                   additive(shifted, None, rate, start, max_harmonics, wave))
    return output / math.sqrt(voices)


def fm(frequency, count, rate, ratio, index, start):
    frequency, count = hz(frequency, count, rate)
    ratio = curve(ratio, count, "ratio", low=0.0)
    index = curve(index, count, "index")
    modulator = np.sin(phase(frequency * ratio, count, rate, 0.0))
    return np.sin(phase(frequency, count, rate, start) + index * modulator)


def noise(rng, count, rate, color):
    choice(color, "color", ("white", "pink", "brown"))
    if color == "white" or count < 2:
        return rng.uniform(-1.0, 1.0, count)
    spectrum = np.fft.rfft(rng.normal(0.0, 1.0, count))
    bins = np.arange(len(spectrum), dtype=float)
    # Power falls as 1/f (pink) or 1/f^2 (brown); below ~10 Hz the slope flattens and DC is removed.
    floor = max(1.0, 10.0 * count / rate)
    spectrum /= np.maximum(bins, floor) ** (0.5 if color == "pink" else 1.0)
    spectrum[0] = 0.0
    result = np.fft.irfft(spectrum, count)
    peak = np.max(np.abs(result))
    return result / peak if peak else result


def modal(rng, count, rate, frequency, ratios, decay, gains, falloff, attack, random_phase, spread, click):
    frequency = positive(frequency, "frequency")
    ratios = np.asarray(ratios, dtype=float)
    if ratios.ndim != 1 or not len(ratios) or not np.isfinite(ratios).all() or np.any(ratios <= 0):
        raise ValueError("ratios must be a nonempty list of positive numbers")
    spread = unit(spread, "spread", closed=False)
    if spread:
        # Every strike is slightly different: each partial is detuned by up to +/- spread.
        ratios = ratios * (1.0 + rng.uniform(-spread, spread, len(ratios)))
    decays = np.asarray(decay, dtype=float)
    if decays.ndim == 0:
        decays = positive(decay, "decay") / (1.0 + nonnegative(falloff, "falloff") * np.arange(len(ratios)))
    elif decays.shape != ratios.shape or np.any(decays <= 0) or not np.isfinite(decays).all():
        raise ValueError("decay must be positive seconds, or one per ratio")
    gains = 1.0 / (1.0 + np.arange(len(ratios))) if gains is None else np.asarray(gains, dtype=float)
    if gains.shape != ratios.shape or not np.isfinite(gains).all():
        raise ValueError("gains must have one finite value per ratio")
    t = np.arange(count) / rate
    offsets = rng.uniform(0.0, 2.0 * math.pi, len(ratios)) if random_phase else np.zeros(len(ratios))
    output = np.zeros(count)
    for ratio, life, gain, offset in zip(ratios, decays, gains, offsets):
        if frequency * ratio < rate / 2:
            output += gain * np.sin(2.0 * math.pi * frequency * ratio * t + offset) * np.exp(-t / life)
    attack = nonnegative(attack, "attack")
    output *= np.minimum(t / attack, 1.0) if attack else 1.0
    click = nonnegative(click, "click")
    if click and count:
        # Contact noise of the strike: a 6 ms band-passed burst above 2 kHz.
        output += click * burst(rng, count, rate, None, (2000.0, min(9000.0, rate * 0.45)), None, 0.006, 0.0, "white")
    return output


def burst(rng, count, rate, lowpass, band, highpass, decay, attack, color):
    """Filtered noise with a decaying envelope: hits, clicks, crackle grains, breath, rumble."""
    if sum(value is not None for value in (lowpass, band, highpass)) > 1:
        raise ValueError("burst takes at most one of lowpass, band or highpass")
    signal = noise(rng, count, rate, color)
    if band is not None:
        low, high = band
        signal = bandpass(signal, low, high, 2, rate)
    elif lowpass is not None:
        signal = butterworth(signal, positive(lowpass, "lowpass"), "lowpass", 2, rate)
    elif highpass is not None:
        signal = butterworth(signal, positive(highpass, "highpass"), "highpass", 2, rate)
    return signal * decay_curve(count, rate, decay, attack, 0.0)


def crackle(rng, count, rate, grains, band, within, length, decay, gain, envelope):
    """`grains` short noise grains at random onsets, summed, then band-passed (fire, ice, gravel, sparks)."""
    grains = operator.index(grains)
    if grains < 0:
        raise ValueError("grains must not be negative")
    seconds = count / rate
    if callable(within):
        onset = within
    else:
        start, end = (0.0, seconds) if within is None else within
        if not 0 <= start <= end:
            raise ValueError("within must be (start, end) seconds with 0 <= start <= end")
        onset = lambda: rng.uniform(start, end)
    shortest, longest = length
    low_gain, high_gain = gain
    if not 0 < shortest <= longest:
        raise ValueError("length must be (shortest, longest) positive seconds")
    if envelope is not None:
        envelope = curve(envelope, count, "envelope")
    output = np.zeros(count)
    for _ in range(grains):
        at = min(count, max(0, round(onset() * rate)))
        size = max(1, round(rng.uniform(shortest, longest) * rate))
        grain = rng.uniform(-1.0, 1.0, size) * decay_curve(size, rate, decay, 0.0, 0.0)
        grain *= rng.uniform(low_gain, high_gain) * (envelope[min(at, count - 1)] if envelope is not None else 1.0)
        end = min(count, at + size)
        output[at:end] += grain[:end - at]
    low, high = band
    return bandpass(output, low, high, 2, rate) if count else output


def thump(rng, count, rate, start, end, sweep, decay, click):
    body = sine(glide(count, rate, positive(start, "start"), positive(end, "end"), sweep), None, rate, 0.0)
    body *= decay_curve(count, rate, decay, 0.0, 0.0)
    click = nonnegative(click, "click")
    if click and count:
        burst = butterworth(rng.uniform(-1.0, 1.0, count), min(6000.0, rate * 0.45), "lowpass", 2, rate)
        body += click * burst * np.exp(-np.arange(count) / (0.004 * rate))
    return body


def pluck(rng, count, rate, frequency, decay, brightness):
    """Karplus-Strong string, tuned exactly with a first-order all-pass fractional delay."""
    frequency = positive(frequency, "frequency")
    decay = positive(decay, "decay")
    brightness = unit(brightness, "brightness")
    if frequency < 20 or frequency >= rate / 4:
        raise ValueError("pluck frequency must be between 20 Hz and a quarter of the sample rate")
    # Loop delay = period + 0.5 (two-point average) + fraction (all-pass), with the fraction in [0.1, 1.1).
    total = rate / frequency - 0.5
    period = int(math.floor(total - 0.1))
    fraction = total - period
    coefficient = (1.0 - fraction) / (1.0 + fraction)
    excitation = rng.uniform(-1.0, 1.0, period)
    excitation = brightness * excitation + (1.0 - brightness) * 0.5 * (excitation + np.roll(excitation, 1))
    # Loop gain per round trip gives a -60 dB decay after `decay` seconds.
    loop = 10.0 ** (-3.0 * (rate / frequency) / (rate * decay))
    line = excitation.tolist()
    output = [0.0] * count
    last = previous_in = previous_out = 0.0
    for index in range(count):
        value = line[index % period]
        output[index] = value
        averaged = loop * 0.5 * (value + last)
        allpassed = coefficient * averaged + previous_in - coefficient * previous_out
        last, previous_in, previous_out = value, averaged, allpassed
        line[index % period] = allpassed
    output = np.array(output)
    return output - np.mean(output) if count else output
