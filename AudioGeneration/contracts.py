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


@dataclass(frozen=True)
class AudioReviewClip:
    """Owned signal, aligned stems and provenance captured for an offline review."""

    samples: object
    sample_rate: int
    layers: dict
    layer_metadata: dict
    random: dict
    parameters: dict
    source: dict
    _source_path: object = None
