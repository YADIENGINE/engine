"""Portable text and strict JSON reports for audio and named layers."""

import json
from pathlib import Path


def level(value):
    return f"{value:.2f}" if value is not None else "n/a"


def audio_lines(report):
    lines = [f"Duration: {report['duration_seconds']:.3f} s ({report['sample_count']} samples, "
             f"{report['sample_rate']} Hz)",
             f"Peak: {level(report['peak_dbfs'])} dBFS | RMS: {level(report['rms_dbfs'])} dBFS | "
             f"Crest: {level(report['crest_db'])} dB | Loudness: {level(report['loudness_lufs'])} LUFS",
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
    if report["kind"] == "comparison":
        lines = [f"Reference comparison | Focus: {report['focus']}"]
        for side in ("reference", "candidate"):
            marks = report[side]["landmarks"]
            lines.extend(["", side.title(), f"Detected onset: {level(marks['onset_seconds'])} s",
                          *audio_lines(report[side]["raw"])])
            if report[side]["source"]:
                lines.append("Source: " + json.dumps(report[side]["source"], ensure_ascii=False))
            if report[side]["parameters"]:
                lines.append("Recipe parameters: " + json.dumps(report[side]["parameters"], ensure_ascii=False))
        lines.extend(["", "Active-stage differences (candidate minus reference)"])
        for metric in report["metrics"][report["focus"]].values():
            lines.append(f"{metric['label']}: {level(metric['reference'])} -> {level(metric['candidate'])} "
                         f"{metric['unit']} | delta {level(metric['delta'])}")
        lines.extend(["", *[finding["message"] for finding in report["differences"]], "", *report["notes"]])
        return "\n".join(lines) + "\n"
    if report["kind"] == "review":
        lines = [report["title"], "Local reference listening session",
                 f"Raw playback common gain: {level(report['settings']['raw_gain_db'])} dB"]
        for key, comparison in report["comparisons"].items():
            lines.extend(["", report["clips"][key]["name"], format_report(comparison)])
        return "\n".join(lines) + "\n"
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
