"""Self-contained listening page for a set of finished sounds (no reference recording needed)."""

from pathlib import Path

import numpy as np

from . import review_io, review_visuals
from .wave_io import pcm16


def export(generator, path, sounds, title):
    if not isinstance(title, str) or not title.strip():
        raise ValueError("title must be a nonempty string")
    if not sounds:
        raise ValueError("A gallery needs at least one sound")
    rate = generator.sample_rate
    entries = []
    for name, samples in sounds.items():
        if not isinstance(name, str) or not name:
            raise ValueError("Sound names must be nonempty strings")
        samples = review_io.snapshot(samples)
        report = generator.analyze(samples)
        entries.append({"name": name, "duration": len(samples) / rate,
                        "peak_dbfs": report["peak_dbfs"], "rms_dbfs": report["rms_dbfs"],
                        "loudness_lufs": report["loudness_lufs"],
                        "warnings": [warning["message"] for warning in report["warnings"]],
                        # Played at its true level so the set can be compared by ear.
                        "audio": review_io.data_uri(pcm16(samples if len(samples) else np.zeros(1), rate)),
                        "plots": review_visuals.plots(samples, rate)})
    html = (Path(__file__).parent / "gallery_page.html").read_text(encoding="utf-8")
    html = html.replace("@@DATA@@", review_io.embedded_json({"title": title, "sounds": entries}))
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(html, encoding="utf-8")
    return path
