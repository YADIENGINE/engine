"""Self-contained listening page for a set of finished sounds (no reference recording needed).

Entries may carry the file currently on disk (A/B against the new take) and a group/choice, so seed and
parameter explorations can be picked in the page and saved as a JSON selection.
"""

from pathlib import Path

import numpy as np

from . import review_io, review_visuals
from .wave_io import pcm


def _take(generator, samples):
    samples = review_io.snapshot(samples)
    report = generator.analyze(samples)
    return {"duration": len(samples) / generator.sample_rate, "peak_dbfs": report["peak_dbfs"],
            "rms_dbfs": report["rms_dbfs"], "loudness_lufs": report["loudness_lufs"],
            "warnings": [warning["message"] for warning in report["warnings"]],
            # Played at its true level so the set can be compared by ear.
            "audio": review_io.data_uri(pcm(samples if len(samples) else np.zeros(1), generator.sample_rate)),
            "plots": review_visuals.plots(samples, generator.sample_rate)}


def entries_of(sounds, current):
    current = current or {}
    unknown = set(current) - set(sounds)
    if unknown:
        raise ValueError(f"current has sounds that are not in the gallery: {', '.join(sorted(unknown))}")
    return [{"name": name, "group": name, "samples": samples, "current": current.get(name), "choice": None}
            for name, samples in sounds.items()]


def export(generator, path, entries, title):
    if not isinstance(title, str) or not title.strip():
        raise ValueError("title must be a nonempty string")
    if not entries:
        raise ValueError("A gallery needs at least one sound")
    page = []
    for entry in entries:
        if not isinstance(entry["name"], str) or not entry["name"]:
            raise ValueError("Sound names must be nonempty strings")
        item = {"name": entry["name"], "group": entry["group"], "choice": entry["choice"],
                "take": _take(generator, entry["samples"]),
                "current": _take(generator, entry["current"]) if entry["current"] is not None else None}
        page.append(item)
    html = (Path(__file__).parent / "gallery_page.html").read_text(encoding="utf-8")
    html = html.replace("@@DATA@@", review_io.embedded_json({"title": title, "sounds": page}))
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(html, encoding="utf-8")
    return path
