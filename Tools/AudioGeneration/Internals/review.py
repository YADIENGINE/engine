"""Self-contained local listening sessions assembled from public review clips."""

import json
import math
from pathlib import Path

import numpy as np

from . import comparison, review_io, review_visuals
from .validation import positive_integer


def peak(samples):
    return float(np.max(np.abs(samples))) if len(samples) else 0.0


def export(generator, directory, reference, variants, title, focus, align, match,
           target_dbfs, max_gain_db, silence_dbfs, onset_relative_db, frame_seconds):
    comparison.options(focus, silence_dbfs, onset_relative_db, frame_seconds)
    rate = positive_integer(generator.sample_rate, "sample_rate")
    if not isinstance(title, str) or not title.strip():
        raise ValueError("title must be a nonempty string")
    if match not in ("raw", "rms", "peak"):
        raise ValueError("match must be raw, rms or peak")
    if not math.isfinite(target_dbfs) or target_dbfs > 0:
        raise ValueError("target_dbfs must be finite and at or below zero")
    if not math.isfinite(max_gain_db) or not 0 <= max_gain_db <= 60:
        raise ValueError("max_gain_db must be between 0 and 60 dB")
    if not variants:
        raise ValueError("At least one named variant is required")
    clips = {"reference": ("Reference", review_io.coerce(generator, reference))}
    for index, (name, value) in enumerate(variants.items(), 1):
        if not isinstance(name, str) or not name.strip():
            raise ValueError("Variant names must be nonempty strings")
        clips[f"variant-{index:03d}"] = (name, review_io.coerce(generator, value))
    # Validate every input before creating the session, including raw WAV range.
    for name, clip in clips.values():
        for samples in [clip.samples, *clip.layers.values()]:
            if not np.isfinite(samples).all() or peak(samples) > np.finfo(np.float32).max:
                raise ValueError(f"{name}: raw WAV export requires finite float32-range samples")
        review_io.json_copy({"parameters": clip.parameters, "random": clip.random,
                             "layer_metadata": clip.layer_metadata, "source": clip.source})
    features = {key: comparison.measure(generator, clip, silence_dbfs, onset_relative_db, frame_seconds)
                for key, (_, clip) in clips.items()}
    comparisons = {key: comparison.compare_features(features["reference"], features[key], focus, align,
                                                    silence_dbfs, onset_relative_db)
                   for key in clips if key != "reference"}
    all_peaks = {key: max([peak(clip.samples), *[peak(stem) for stem in clip.layers.values()]])
                 for key, (_, clip) in clips.items()}
    highest = max(all_peaks.values())
    raw_gain = min(1.0, 0.89 / highest) if highest else 1.0
    session = {"schema_version": 1, "kind": "review", "title": title,
               "settings": {"focus": focus, "align": bool(align), "match": match,
                            "target_dbfs": target_dbfs, "max_gain_db": max_gain_db,
                            "raw_gain": raw_gain, "raw_gain_db": comparison.db(raw_gain)},
               "comparisons": comparisons, "clips": {}}
    directory = Path(directory)
    files = {}
    page_clips = []
    for key, (name, clip) in clips.items():
        matching = {method: comparison.matching(features[key], method, target_dbfs, max_gain_db, all_peaks[key])
                    for method in ("rms", "peak")}
        tracks = {}
        for index, (layer_name, samples) in enumerate([(None, clip.samples), *clip.layers.items()]):
            track_id = key if layer_name is None else f"{key}-layer-{index:03d}"
            payload, encoding_gain, data_uri = review_io.pcm_preview(samples, rate)
            raw_path = directory / "raw" / f"{track_id}.wav"
            preview_path = directory / "audio" / f"{track_id}.wav"
            layer_analysis = None
            if layer_name is not None:
                layer_analysis = generator.analyze(samples, silence_dbfs=silence_dbfs, frame_seconds=frame_seconds)
                layer_analysis["random"] = clip.layer_metadata[layer_name]["random"]
            tracks[track_id] = {"label": "Mix" if layer_name is None else layer_name,
                                "layer_name": layer_name, "data_uri": data_uri,
                                "encoding_gain": encoding_gain, "peak": peak(samples),
                                "raw_path": raw_path.relative_to(directory).as_posix(),
                                "preview_path": preview_path.relative_to(directory).as_posix(),
                                "metadata": clip.layer_metadata.get(layer_name) if layer_name else None,
                                "analysis": layer_analysis}
            files[track_id] = (raw_path, preview_path, samples, payload)
        session["clips"][key] = {"name": name, "features": features[key], "matching": matching,
                                  "tracks": {tid: {k: v for k, v in track.items() if k != "data_uri"}
                                             for tid, track in tracks.items()}}
        page_clips.append({"id": key, "name": name, "duration": len(clip.samples) / rate,
                           "features": features[key], "matching": matching, "tracks": tracks,
                           "plots": review_visuals.plots(clip.samples, rate)})
    page_data = {"title": title, "settings": session["settings"], "clips": page_clips,
                 "comparisons": comparisons}
    serialized = review_io.embedded_json(page_data)
    assets = Path(__file__).parent
    html = (assets / "review_page.html").read_text(encoding="utf-8")
    html = html.replace("@@CSS@@", (assets / "review_page.css").read_text(encoding="utf-8"))
    html = html.replace("@@SCRIPT@@", (assets / "review_page.js").read_text(encoding="utf-8"))
    html = html.replace("@@DATA@@", serialized)
    # Refuse an output layout that would overwrite any source recording.
    planned = [directory / "index.html", directory / "review.json", directory / "review.txt"]
    planned += [path for raw, preview, _, _ in files.values() for path in (raw, preview)]
    source_paths = {clip._source_path for _, clip in clips.values() if clip._source_path is not None}
    if any(path.resolve() in source_paths for path in planned):
        raise ValueError("Review output would overwrite a source recording; choose another directory")
    # Validate the complete report before writing any files.
    json.dumps(session, allow_nan=False)
    generator.format_report(session)
    for raw, preview, samples, payload in files.values():
        raw.parent.mkdir(parents=True, exist_ok=True)
        preview.parent.mkdir(parents=True, exist_ok=True)
        review_io.write_raw(raw, samples, rate)
        preview.write_bytes(payload)
    paths = generator.write_report(directory / "review", session)
    index_path = directory / "index.html"
    index_path.write_text(html, encoding="utf-8")
    return {"html": index_path, "report": paths,
            "audio": {key: {"raw": raw, "preview": preview}
                      for key, (raw, preview, _, _) in files.items()}}
