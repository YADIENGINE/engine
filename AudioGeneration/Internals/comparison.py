"""Alignment landmarks, stage measurements and factual reference differences."""

import math

import numpy as np

from .analysis import db

FOCUSES = ("all", "attack", "body", "tail")


def options(focus, silence_dbfs, onset_relative_db, frame_seconds):
    if focus not in FOCUSES:
        raise ValueError("focus must be all, attack, body or tail")
    for name, value in (("silence_dbfs", silence_dbfs), ("onset_relative_db", onset_relative_db)):
        if not math.isfinite(value) or value > 0:
            raise ValueError(f"{name} must be finite and at or below zero")
    if not math.isfinite(frame_seconds) or frame_seconds <= 0:
        raise ValueError("frame_seconds must be positive and finite")


def landmarks(samples, rate, silence_dbfs, onset_relative_db):
    count = len(samples)
    empty = {"onset_sample": None, "end_sample": None, "onset_seconds": None,
             "end_seconds": None, "rise_10_90_seconds": None, "peak_at_seconds": None,
             "segments": {name: [0, 0] for name in FOCUSES}}
    peak = float(np.max(np.abs(samples))) if count else 0.0
    if not peak or db(peak) <= silence_dbfs:
        return empty
    scaled = samples / peak
    window = max(1, round(0.001 * rate))
    energy = np.r_[0.0, np.cumsum(scaled ** 2)]
    starts = np.maximum(0, np.arange(count) + 1 - window)
    envelope = np.sqrt(np.maximum(0.0, energy[1:] - energy[starts]) / window)
    threshold_db = max(silence_dbfs - db(peak), onset_relative_db)
    active = np.flatnonzero(envelope > 10 ** (threshold_db / 20))
    if not len(active):
        return empty
    start, end = int(active[0]), int(active[-1]) + 1
    dominant = start + int(np.argmax(envelope[start:end]))
    envelope_peak = envelope[dominant]
    rising = envelope[start:dominant + 1]
    ten = start + int(np.flatnonzero(rising >= envelope_peak * 0.1)[0])
    ninety = start + int(np.flatnonzero(rising >= envelope_peak * 0.9)[0])
    attack_end = min(end, ninety + 1)
    cumulative = np.cumsum(scaled[start:end] ** 2)
    tail_start = start + int(np.searchsorted(cumulative, cumulative[-1] * 0.9)) + 1
    tail_start = min(end, max(attack_end, tail_start))
    return {"onset_sample": start, "end_sample": end, "onset_seconds": start / rate,
            "end_seconds": end / rate, "rise_10_90_seconds": (ninety - ten) / rate,
            "peak_at_seconds": (dominant - start) / rate,
            "segments": {"all": [start, end], "attack": [start, attack_end],
                         "body": [attack_end, tail_start], "tail": [tail_start, end]}}


def measure(generator, clip, silence_dbfs, onset_relative_db, frame_seconds):
    marks = landmarks(clip.samples, clip.sample_rate, silence_dbfs, onset_relative_db)
    raw = generator.analyze(clip.samples, silence_dbfs=silence_dbfs, frame_seconds=frame_seconds)
    raw["random"] = clip.random
    stages = {}
    for name, (start, end) in marks["segments"].items():
        stages[name] = generator.analyze(clip.samples[start:end], silence_dbfs=silence_dbfs,
                                        frame_seconds=frame_seconds)
        stages[name]["random"] = clip.random
    return {"raw": raw, "landmarks": marks, "stages": stages,
            "parameters": clip.parameters, "random": clip.random, "source": clip.source}


def finite_delta(candidate, reference):
    if candidate is None or reference is None:
        return None
    value = candidate - reference
    return float(value) if math.isfinite(value) else None


def metrics(reference, candidate, stage):
    a, b = reference["stages"][stage], candidate["stages"][stage]
    result = {}

    def add(key, label, unit, av, bv, tolerance):
        result[key] = {"label": label, "unit": unit, "reference": av, "candidate": bv,
                       "delta": finite_delta(bv, av), "notice_threshold": tolerance}

    for key, label, unit, tolerance in (("duration_seconds", "Duration", "s", 0.005),
                                        ("peak_dbfs", "Peak", "dB", 0.5),
                                        ("rms_dbfs", "RMS", "dB", 0.5),
                                        ("crest_db", "Crest factor", "dB", 0.5),
                                        ("dc_offset", "DC offset", "amplitude", 0.01)):
        # Empty stages are unavailable, rather than a fictitious zero-duration match.
        add(key, label, unit, a[key] if a["sample_count"] else None,
            b[key] if b["sample_count"] else None, tolerance)
    for key, label, unit, tolerance in (("centroid_hz", "Spectral centroid", "Hz", 50),
                                        ("flatness", "Spectral flatness", "ratio", 0.05)):
        add(key, label, unit, (a["spectral"] or {}).get(key), (b["spectral"] or {}).get(key), tolerance)
    for band in ("sub", "bass", "mid", "treble"):
        av = ((a["spectral"] or {}).get("band_energy_ratio") or {}).get(band)
        bv = ((b["spectral"] or {}).get("band_energy_ratio") or {}).get(band)
        add("band_" + band, band.title() + " energy", "pp",
            av * 100 if av is not None else None, bv * 100 if bv is not None else None, 5)
    if stage == "all":
        for key, label in (("rise_10_90_seconds", "10-90% rise"), ("peak_at_seconds", "Time to dominant peak")):
            add(key, label, "s", reference["landmarks"][key], candidate["landmarks"][key], 0.005)
    return result


def compare_features(reference, candidate, focus, align, silence_dbfs, onset_relative_db):
    comparisons = {stage: metrics(reference, candidate, stage) for stage in FOCUSES}
    differences = []
    for key, metric in comparisons[focus].items():
        delta = metric["delta"]
        if delta is not None and abs(delta) >= metric["notice_threshold"]:
            direction = ("longer" if delta > 0 else "shorter") if metric["unit"] == "s" else (
                "higher" if delta > 0 else "lower")
            differences.append({"metric": key, "delta": delta, "unit": metric["unit"],
                                "message": f"{metric['label']}: candidate is {abs(delta):.3g} "
                                           f"{metric['unit']} {direction}."})
    return {"schema_version": 1, "kind": "comparison", "focus": focus,
            "alignment": {"enabled": bool(align), "method": "1 ms trailing RMS onset",
                          "silence_dbfs": silence_dbfs, "onset_relative_db": onset_relative_db,
                          "reference_offset_seconds": reference["landmarks"]["onset_seconds"] if align else 0.0,
                          "candidate_offset_seconds": candidate["landmarks"]["onset_seconds"] if align else 0.0},
            "reference": reference, "candidate": candidate, "metrics": comparisons,
            "differences": differences,
            "notes": ["Deltas are candidate minus reference, measured before playback gain.",
                      "Active stages exclude detected leading/trailing silence; signals are never time-stretched.",
                      "Thresholds flag measurable differences, not quality or perceptual similarity."]}


def matching(features, method, target_dbfs, max_gain_db, preview_peak):
    stage = features["stages"]["all"]
    level = stage["rms_dbfs"] if method == "rms" else stage["peak_dbfs"]
    if level is None:
        requested = 0.0
    else:
        requested = min(max_gain_db, target_dbfs - level)
    headroom_db = db(0.89)
    ceiling = headroom_db - db(preview_peak) if preview_peak else 0.0
    applied = min(requested, ceiling)
    return {"gain_db": applied, "gain": 10 ** (applied / 20),
            "requested_gain_db": requested, "target_dbfs": target_dbfs,
            "limited_by_peak": applied < requested,
            "limited_by_gain": level is not None and target_dbfs - level > max_gain_db,
            "measured_level_dbfs": level,
            "result_level_dbfs": level + applied if level is not None else None}


def compare(generator, reference, candidate, focus, align, silence_dbfs, onset_relative_db, frame_seconds):
    from .review_io import coerce

    options(focus, silence_dbfs, onset_relative_db, frame_seconds)
    a, b = coerce(generator, reference), coerce(generator, candidate)
    af = measure(generator, a, silence_dbfs, onset_relative_db, frame_seconds)
    bf = measure(generator, b, silence_dbfs, onset_relative_db, frame_seconds)
    return compare_features(af, bf, focus, align, silence_dbfs, onset_relative_db)
