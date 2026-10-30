"""Placing sounds in time: layers, mixes, padding, slicing and randomly scattered grains."""

import numpy as np

from ..contracts import AudioLayer
from .validation import finite, mono, nonnegative


def pad(samples, count):
    return np.concatenate([mono(samples), np.zeros(count)])


def segment(samples, count, offset):
    samples = mono(samples)
    result = np.zeros(count)
    source = samples[offset:offset + count]
    result[:len(source)] = source
    return result


def loop(samples, count):
    """Seamless loop: the last `count` samples cross-fade (equal power) into the start; result is shorter by count."""
    samples = mono(samples)
    if not 0 < count < len(samples) / 2:
        raise ValueError("crossfade must be positive and shorter than half the sound")
    result = samples[:-count].copy()
    angle = np.linspace(0.0, np.pi / 2, count, endpoint=False)
    result[:count] = samples[:count] * np.sin(angle) + samples[-count:] * np.cos(angle)
    return result


def layer(samples, rate, at, gain, random):
    samples = mono(samples)
    samples.setflags(write=False)
    return AudioLayer(samples, rate, nonnegative(at, "at"), finite(gain, "gain"), random)


def _entries(generator, layers):
    if isinstance(layers, dict):
        items = list(layers.items())
        for name, _ in items:
            if not isinstance(name, str) or not name.strip():
                raise ValueError("Layer names must be nonempty strings")
    else:
        items = [(f"layer {index}", value) for index, value in enumerate(layers, 1)]
    if not items:
        raise ValueError("mix needs at least one layer")
    prepared = []
    for name, value in items:
        if isinstance(value, tuple):
            if len(value) not in (2, 3):
                raise ValueError("A timed layer is (at, samples) or (at, samples, gain)")
            value = generator.layer(value[1], at=value[0], gain=value[2] if len(value) == 3 else 1.0)
        elif not isinstance(value, AudioLayer):
            value = generator.layer(value)
        if value.sample_rate != generator.sample_rate:
            raise ValueError(f"Layer {name!r} has a different sample rate")
        prepared.append((name, value))
    return prepared


def render(generator, layers, seconds):
    """Mix plus aligned, gain-applied stems and their placement metadata."""
    prepared = _entries(generator, layers)
    starts = [generator.sample_count(value.at_seconds) for _, value in prepared]
    count = (generator.sample_count(nonnegative(seconds, "seconds")) if seconds is not None else
             max(start + len(value.samples) for start, (_, value) in zip(starts, prepared)))
    mix = np.zeros(count)
    stems, metadata = {}, {}
    for start, (name, value) in zip(starts, prepared):
        retained = min(len(value.samples), max(count - start, 0))
        stem = np.zeros(count)
        stem[start:start + retained] = value.samples[:retained] * value.gain
        mix += stem
        stems[name] = stem
        metadata[name] = {"at_seconds": value.at_seconds, "gain": value.gain,
                          "source_sample_count": len(value.samples), "retained_sample_count": retained,
                          "random": value.random}
    return mix, stems, metadata


def scatter(rng, count, rate, density, grain, gain):
    """round(density * seconds) grains at uniform random onsets; grains crossing the end are cropped."""
    density = nonnegative(density, "density")
    if gain is not None:
        gain = mono(gain, "gain")
        if len(gain) != count:
            raise ValueError("gain must have one value per output sample")
    output = np.zeros(count)
    for _ in range(round(density * count / rate)):
        at = int(rng.integers(0, count)) if count else 0
        piece = mono(grain() if callable(grain) else grain, "grain")
        end = min(count, at + len(piece))
        output[at:end] += piece[:end - at] * (gain[at] if gain is not None else 1.0)
    return output

