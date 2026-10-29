"""Named layers and aligned stems, without normalization or implicit trimming."""

import math
from pathlib import Path
import re

import numpy as np

from ..contracts import AudioLayer
from .analysis import mono


def layer(samples, sample_rate, at, gain, random):
    if not math.isfinite(at) or at < 0:
        raise ValueError("Layer start must be nonnegative and finite")
    if not math.isfinite(gain):
        raise ValueError("Layer gain must be finite")
    samples = mono(samples).copy()
    samples.setflags(write=False)
    return AudioLayer(samples, sample_rate, float(at), float(gain), random)


def render(generator, layers, seconds):
    if not layers:
        raise ValueError("At least one named layer is required")
    if not math.isfinite(generator.sample_rate) or generator.sample_rate <= 0:
        raise ValueError("sample_rate must be positive and finite")
    prepared = {}
    for name, value in layers.items():
        if not isinstance(name, str) or not name.strip():
            raise ValueError("Layer names must be nonempty strings")
        if not isinstance(value, AudioLayer):
            value = generator.layer(value)
        if value.sample_rate != generator.sample_rate:
            raise ValueError(f"Layer {name!r} has a different sample rate")
        prepared[name] = value
    if seconds is not None and (not math.isfinite(seconds) or seconds < 0):
        raise ValueError("Mix duration must be nonnegative and finite")
    count = (generator.sample_count(seconds) if seconds is not None else
             max(generator.sample_count(value.at_seconds) + len(value.samples) for value in prepared.values()))
    mix = np.zeros(count)
    stems = {}
    metadata = {}
    for name, value in prepared.items():
        start = generator.sample_count(value.at_seconds)
        retained = min(len(value.samples), max(count - start, 0))
        stem = np.zeros(count)
        if retained:
            stem[start:start + retained] = value.samples[:retained] * value.gain
        mix += stem
        stems[name] = stem
        metadata[name] = {"at_seconds": value.at_seconds, "gain": value.gain,
                          "source_sample_count": len(value.samples), "retained_sample_count": retained,
                          "random": value.random}
    return mix, stems, metadata


def analyze(generator, mix, stems, metadata, options):
    report = {"schema_version": 1, "kind": "layers", "mix": generator.analyze(mix, **options), "layers": {}}
    for name, stem in stems.items():
        info = metadata[name].copy()
        info["analysis"] = generator.analyze(stem, **options)
        info["analysis"]["random"] = info.pop("random")
        report["layers"][name] = info
    return report


def export(generator, directory, mix, stems, metadata, report):
    # Do not quantize invalid samples into plausible-looking audio files.
    if not np.isfinite(mix).all() or any(not np.isfinite(stem).all() for stem in stems.values()):
        raise ValueError("Cannot export NaN/Inf layers; inspect them with analyze_layers first")
    directory = Path(directory)
    paths = {"mix": directory / "mix.wav", "layers": {}}
    generator.write_wav(paths["mix"], mix)
    for index, (name, stem) in enumerate(stems.items(), 1):
        label = re.sub(r"[^\w.-]+", "-", name, flags=re.UNICODE).strip(".-")[:80] or "layer"
        # Numbering prevents sanitized-name collisions and Windows reserved names.
        path = directory / "layers" / f"{index:03d}-{label}.wav"
        generator.write_wav(path, stem)
        paths["layers"][name] = path
    if report:
        paths["report"] = generator.write_report(directory / "analysis", analyze(generator, mix, stems, metadata, {}))
    return paths
