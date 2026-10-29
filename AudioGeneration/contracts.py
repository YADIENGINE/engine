"""Returned value contracts. Construct layers through SoundGenerator.layer()."""

from dataclasses import dataclass


@dataclass(frozen=True)
class AudioLayer:
    """Owned read-only mono sample snapshot and its timeline placement."""

    samples: object
    sample_rate: int
    at_seconds: float
    gain: float
    random: dict
