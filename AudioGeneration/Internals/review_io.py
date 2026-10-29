"""Review snapshots and audio encodings; original source files are never written."""

import base64
import json
import os
from pathlib import Path

import numpy as np

from ..contracts import AudioReviewClip
from .validation import mono, positive_integer
from .wave_io import load, pcm


def json_copy(value):
    return json.loads(json.dumps(value, ensure_ascii=False, allow_nan=False))


def browser_numbers(value):
    """Keep exact large seeds/parameters when JSON is parsed by JavaScript."""
    if isinstance(value, int) and not isinstance(value, bool) and abs(value) > 2 ** 53 - 1:
        return str(value)
    if isinstance(value, dict):
        return {key: browser_numbers(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [browser_numbers(item) for item in value]
    return value


def embedded_json(value):
    """Compact JSON safe inside an HTML <script> element, with exact large integers for JavaScript."""
    serialized = json.dumps(browser_numbers(value), ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    # Embedded JSON is data, even when a label contains an HTML closing tag.
    for character in "&<>\u2028\u2029":
        serialized = serialized.replace(character, "\\u%04x" % ord(character))
    return serialized


def snapshot(samples):
    samples = mono(samples)
    samples.setflags(write=False)
    return samples


def clip(generator, source, layers, seconds, parameters):
    rate = positive_integer(generator.sample_rate, "sample_rate")
    if source is not None and layers is not None:
        raise ValueError("Supply either source or layers, not both")
    if not isinstance(parameters, (dict, type(None))):
        raise ValueError("parameters must be a JSON object")
    if any(not isinstance(key, str) for key in (parameters or {})):
        raise ValueError("Parameter names must be strings")
    parameters = json_copy(parameters or {})
    stems, metadata, source_info = {}, {}, {}
    source_path = None
    if layers is not None:
        from .timeline import render

        samples, stems, metadata = render(generator, layers, seconds)
    else:
        if seconds is not None:
            raise ValueError("seconds applies only to a layered review clip")
        if source is None:
            raise ValueError("Supply a signal, source file or named layers")
        if isinstance(source, (str, os.PathLike)):
            samples, source_info = load(source, rate)
            source_path = Path(source).resolve()
        else:
            samples = source
    random = {"seed": None, "scopes": []} if source_path is not None else generator.seed_info
    return AudioReviewClip(snapshot(samples), rate, {name: snapshot(stem) for name, stem in stems.items()},
                           json_copy(metadata), random, parameters, source_info, source_path)


def coerce(generator, source):
    if isinstance(source, AudioReviewClip):
        if source.sample_rate != generator.sample_rate:
            raise ValueError("Review clips must use the comparison context's sample rate")
        return source
    return generator.review_clip(source)


def pcm_preview(samples, sample_rate):
    peak = float(np.max(np.abs(samples))) if len(samples) else 0.0
    # Encode near full scale for precision; the player restores raw/matched gains.
    gain = min(1e12, 0.89 / peak) if peak > 1e-12 else (1e12 if peak else 1.0)
    # Browsers cannot load a zero-frame WAV. A silent placeholder affects playback only.
    payload = pcm(samples * gain if len(samples) else np.zeros(1), sample_rate)
    return payload, gain, data_uri(payload)


def data_uri(payload):
    return "data:audio/wav;base64," + base64.b64encode(payload).decode("ascii")


def write_raw(path, samples, sample_rate):
    from scipy.io import wavfile

    # Float32 WAV preserves out-of-range mix peaks; PCM16 would silently clip them.
    wavfile.write(path, sample_rate, np.asarray(samples, dtype=np.float32))
