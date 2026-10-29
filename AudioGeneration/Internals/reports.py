"""Portable text and strict JSON reports for audio and named layers."""

import json
from pathlib import Path


def level(value):
    return f"{value:.2f}" if value is not None else "n/a"


def audio_lines(report):
    lines = [f"Duration: {report['duration_seconds']:.3f} s ({report['sample_count']} samples, "
             f"{report['sample_rate']} Hz)",
             f"Peak: {level(report['peak_dbfs'])} dBFS | RMS: {level(report['rms_dbfs'])} dBFS | "
             f"Crest: {level(report['crest_db'])} dB",
             f"Active: {level(report['active_seconds'])} s | Trailing silence: "
             f"{level(report['trailing_silence_seconds'])} s",
             f"DC offset: {report['dc_offset'] if report['dc_offset'] is not None else 'n/a'} | Samples exceeding full scale: "
             f"{report['clipping_sample_count']}"]
    spectral = report.get("spectral")
    if spectral:
        lines.append(f"Spectral centroid: {level(spectral['centroid_hz'])} Hz | "
                     f"Flatness: {level(spectral['flatness'])}")
        if spectral["band_energy_ratio"] is not None:
            lines.append("Band energy: " + ", ".join(f"{name}={ratio:.1%}" for name, ratio in
                                                      spectral["band_energy_ratio"].items()))
    random = report.get("random")
    if random:
        lines.append(f"Seed: {random['seed']} | Scopes: " + json.dumps(random["scopes"], ensure_ascii=False))
    lines.extend(f"[{warning['code']}] {warning['message']}" for warning in report["warnings"])
    return lines


def format_report(report):
    if report["kind"] == "audio":
        return "\n".join(audio_lines(report)) + "\n"
    lines = ["Mix", *audio_lines(report["mix"])]
    for name, layer in report["layers"].items():
        lines.extend(["", f"Layer: {name} | At: {layer['at_seconds']:.3f} s | Gain: {layer['gain']:g}",
                      f"Retained source samples: {layer['retained_sample_count']}/{layer['source_sample_count']}",
                      *audio_lines(layer["analysis"])])
    return "\n".join(lines) + "\n"


def write_report(path, report):
    # Serialize first: reject nonstandard NaN/Infinity without leaving partial reports.
    encoded = json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
    formatted = format_report(report)
    path = Path(path)
    json_path = path.with_suffix(".json")
    text_path = path.with_suffix(".txt")
    json_path.parent.mkdir(parents=True, exist_ok=True)
    json_path.write_text(encoded, encoding="utf-8")
    text_path.write_text(formatted, encoding="utf-8")
    return {"json": json_path, "text": text_path}
