"""Sample buffers, phase integration and in-place layering."""

import numpy as np


def time_axis(count, sample_rate):
    return np.arange(count) / sample_rate


def phase_of(frequency, sample_rate):
    return 2.0 * np.pi * np.cumsum(frequency) / sample_rate


def sweep_phase(t, start, end, seconds, sample_rate):
    frequency = end + (start - end) * np.exp(-t / seconds)
    return phase_of(frequency, sample_rate)


def mix_at(target, sound, start, gain):
    if start >= len(target):
        return target
    end = min(len(target), start + len(sound))
    target[start:end] += sound[:end - start] * gain
    return target


def pad(samples, count):
    return np.concatenate([samples, np.zeros(count)])


def segment(samples, count, offset):
    result = np.zeros(count)
    source = samples[offset:offset + count]
    result[:len(source)] = source
    return result


def normalize(samples, peak, epsilon, additive, divide_first):
    maximum = np.max(np.abs(samples))
    denominator = maximum + epsilon if additive else max(float(maximum), epsilon)
    if divide_first:
        return samples / denominator * peak
    return samples * (peak / denominator)
