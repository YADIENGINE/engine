"""Shared input contracts: every operation validates before it computes."""

import math
import operator

import numpy as np


def mono(samples, name="samples"):
    """A finite, real, one-dimensional float array (always a fresh copy)."""
    values = np.asarray(samples)
    if np.iscomplexobj(values):
        raise ValueError(f"{name} must be real mono samples")
    values = np.array(values, dtype=float)
    if values.ndim != 1:
        raise ValueError(f"{name} must be a one-dimensional mono signal")
    if not np.isfinite(values).all():
        raise ValueError(f"{name} contains NaN or Inf")
    return values


def finite(value, name):
    value = float(value)
    if not math.isfinite(value):
        raise ValueError(f"{name} must be finite")
    return value


def positive(value, name):
    value = finite(value, name)
    if value <= 0:
        raise ValueError(f"{name} must be positive")
    return value


def nonnegative(value, name):
    value = finite(value, name)
    if value < 0:
        raise ValueError(f"{name} must not be negative")
    return value


def unit(value, name, *, closed=True):
    value = finite(value, name)
    if value < 0 or value > 1 or (not closed and value == 1):
        raise ValueError(f"{name} must be between 0 and 1" + ("" if closed else " (exclusive)"))
    return value


def positive_integer(value, name):
    try:
        value = operator.index(value)
    except TypeError as error:
        raise ValueError(f"{name} must be a positive integer") from error
    if value <= 0:
        raise ValueError(f"{name} must be a positive integer")
    return value


def choice(value, name, options):
    if value not in options:
        raise ValueError(f"{name} must be one of: {', '.join(map(str, options))}")
    return value


def curve(value, count, name, *, low=None, high=None):
    """Scalar or one value per sample, broadcast to `count` samples and range checked."""
    if np.iscomplexobj(value):
        raise ValueError(f"{name} must be real")
    values = np.asarray(value, dtype=float)
    if values.ndim > 1 or (values.ndim == 1 and len(values) != count):
        raise ValueError(f"{name} must be a scalar or one value per sample ({count})")
    if not np.isfinite(values).all():
        raise ValueError(f"{name} must be finite")
    if low is not None and np.any(values <= low):
        raise ValueError(f"{name} must stay above {low:g}")
    if high is not None and np.any(values >= high):
        raise ValueError(f"{name} must stay below {high:g}")
    return np.broadcast_to(values, (count,)).astype(float)


def frequency(value, count, sample_rate, name="frequency"):
    """Hz scalar (needs count) or Hz curve; returns (curve, count). Must stay in (0, Nyquist)."""
    if np.iscomplexobj(value):
        raise ValueError(f"{name} must be real Hz")
    values = np.asarray(value, dtype=float)
    if values.ndim == 0:
        if count is None:
            raise ValueError(f"seconds is required when {name} is a scalar")
    elif values.ndim == 1:
        if count is not None and count != len(values):
            raise ValueError(f"seconds does not match the {name} curve length")
        count = len(values)
    else:
        raise ValueError(f"{name} must be scalar Hz or a one-dimensional Hz curve")
    return curve(values, count, name, low=0.0, high=sample_rate / 2), count
