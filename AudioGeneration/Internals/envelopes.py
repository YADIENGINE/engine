"""Existing linear and exponential amplitude shapes."""

import numpy as np


def linear(t, attack, release):
    rise = np.clip(t / attack, 0.0, 1.0)
    fall = np.clip((t[-1] - t) / release, 0.0, 1.0)
    return rise * fall


def exponential(t, attack, decay):
    return (1.0 - np.exp(-t / attack)) * np.exp(-t / decay)


def attack_release(count, attack_count, release_count, curve):
    env = np.ones(count)
    attack_count = min(count, attack_count)
    release_count = min(count - attack_count, release_count)
    if attack_count > 0:
        env[:attack_count] = np.linspace(0.0, 1.0, attack_count) ** curve
    if release_count > 0:
        env[count - release_count:] *= np.linspace(1.0, 0.0, release_count) ** curve
    return env


def fade(samples, attack_count, release_count, curve, copy):
    result = samples.copy() if copy else samples
    attack_count = min(attack_count, len(result))
    release_count = min(release_count, len(result))
    if attack_count:
        result[:attack_count] *= np.linspace(0.0, 1.0, attack_count) ** curve
    if release_count:
        result[-release_count:] *= np.linspace(1.0, 0.0, release_count) ** curve
    return result


def trim_tail(samples, floor, fade_count):
    loud = np.nonzero(np.abs(samples) > floor)[0]
    end = min(len(samples), (loud[-1] if len(loud) else 0) + fade_count)
    result = samples[:end].copy()
    count = min(len(result), fade_count)
    result[-count:] *= np.linspace(1.0, 0.0, count)
    return result
