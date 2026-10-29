"""Input contracts shared by the opt-in quality processing paths."""

import math
import operator

import numpy as np


def finite_mono(samples):
    values = np.asarray(samples)
    if np.iscomplexobj(values):
        raise ValueError("Expected real mono samples")
    values = np.asarray(values, dtype=float)
    if values.ndim != 1 or not np.isfinite(values).all():
        raise ValueError("Expected a finite one-dimensional mono signal")
    return values


def positive(value, name):
    value = float(value)
    if not math.isfinite(value) or value <= 0:
        raise ValueError(f"{name} must be positive and finite")
    return value


def nonnegative(value, name):
    value = float(value)
    if not math.isfinite(value) or value < 0:
        raise ValueError(f"{name} must be nonnegative and finite")
    return value


def positive_integer(value, name):
    try:
        value = operator.index(value)
    except TypeError as error:
        raise ValueError(f"{name} must be a positive integer") from error
    if value <= 0:
        raise ValueError(f"{name} must be a positive integer")
    return value
