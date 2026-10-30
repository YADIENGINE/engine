"""Non-mutating mono signal measurements. No perceptual quality score or mastering."""

import math

import numpy as np


def mono(samples):
    result = np.asarray(samples, dtype=float)
    if result.ndim != 1:
        raise ValueError("Expected a one-dimensional mono signal")
    return result


def db(amplitude):
    return 20.0 * math.log10(amplitude) if amplitude > 0 else None


def rms(samples):
    peak = float(np.max(np.abs(samples))) if len(samples) else 0.0
    return peak * float(np.sqrt(np.mean((samples / peak) ** 2))) if peak else 0.0


def spectrum(samples, sample_rate):
    if not len(samples) or not np.any(samples):
        return {"centroid_hz": None, "flatness": None, "band_energy_ratio": None}
    # Normalize before the FFT to avoid overflow without changing spectral ratios.
    scaled = samples / np.max(np.abs(samples))
    window = np.hanning(len(samples)) if len(samples) > 2 else np.ones(len(samples))
    magnitude = np.abs(np.fft.rfft(scaled * window))
    frequencies = np.fft.rfftfreq(len(samples), 1.0 / sample_rate)
    total_magnitude = float(magnitude.sum())
    if not total_magnitude:
        return {"centroid_hz": None, "flatness": None, "band_energy_ratio": None}
    power = magnitude ** 2
    mean_power = float(power.mean())
    flatness = float(np.exp(np.mean(np.log(np.maximum(power, np.finfo(float).tiny)))) / mean_power)
    # One-sided spectral energy: interior bins represent positive and negative Hz.
    weights = power.copy()
    weights[1:-1 if len(samples) % 2 == 0 else None] *= 2.0
    total_energy = float(weights.sum())
    bands = (("sub", 0.0, 80.0), ("bass", 80.0, 250.0),
             ("mid", 250.0, 4000.0), ("treble", 4000.0, math.inf))
    ratios = {name: float(weights[(frequencies >= low) & (frequencies < high)].sum() / total_energy)
              for name, low, high in bands}
    return {
        "centroid_hz": float(np.sum(magnitude * frequencies) / total_magnitude),
        "flatness": min(1.0, flatness),
        "band_energy_ratio": ratios,
    }


def analyze(samples, sample_rate, silence_dbfs, frame_seconds, edge_dbfs):
    samples = mono(samples)
    sample_rate = float(sample_rate)
    silence_dbfs, frame_seconds, edge_dbfs = map(float, (silence_dbfs, frame_seconds, edge_dbfs))
    if not math.isfinite(sample_rate) or sample_rate <= 0:
        raise ValueError("sample_rate must be positive and finite")
    if not math.isfinite(frame_seconds) or frame_seconds <= 0:
        raise ValueError("frame_seconds must be positive and finite")
    if not math.isfinite(silence_dbfs) or not math.isfinite(edge_dbfs):
        raise ValueError("Analysis thresholds must be finite")
    if silence_dbfs > 0 or edge_dbfs > 0:
        raise ValueError("Analysis thresholds must be at or below 0 dBFS")
    count = len(samples)
    finite = np.isfinite(samples)
    invalid_count = int(np.count_nonzero(~finite))
    clipped_count = int(np.count_nonzero(finite & (np.abs(samples) > 1.0)))
    report = {
        "schema_version": 1,
        "kind": "audio",
        "sample_rate": sample_rate,
        "sample_count": count,
        "duration_seconds": count / sample_rate,
        "thresholds": {"silence_dbfs": silence_dbfs, "edge_dbfs": edge_dbfs,
                       "frame_seconds": frame_seconds},
        "invalid_samples": {"nan": int(np.count_nonzero(np.isnan(samples))),
                            "positive_inf": int(np.count_nonzero(np.isposinf(samples))),
                            "negative_inf": int(np.count_nonzero(np.isneginf(samples)))},
        "clipping_sample_count": clipped_count,
        "peak_amplitude": None,
        "peak_dbfs": None,
        "rms_amplitude": None,
        "rms_dbfs": None,
        "crest_db": None,
        "loudness_lufs": None,
        "dc_offset": None,
        "active_seconds": None,
        "active_span_seconds": None,
        "leading_silence_seconds": None,
        "trailing_silence_seconds": None,
        "edges": None,
        "spectral": None,
        "frames": [],
        "warnings": [],
    }
    warnings = report["warnings"]
    if clipped_count:
        warnings.append({"code": "clipping", "message": f"{clipped_count} samples exceed full scale before export."})
    if invalid_count:
        warnings.append({"code": "non_finite", "message": f"{invalid_count} NaN/Inf samples; derived metrics unavailable."})
        return report
    if not count:
        warnings.append({"code": "empty", "message": "Signal has no samples."})
        return report
    peak = float(np.max(np.abs(samples)))
    energy = rms(samples)
    report.update(peak_amplitude=peak, peak_dbfs=db(peak), rms_amplitude=energy, rms_dbfs=db(energy),
                  crest_db=20 * (math.log10(peak) - math.log10(energy)) if energy else None,
                  dc_offset=peak * float(np.mean(samples / peak)) if peak else 0.0)
    if peak:
        from .level import loudness

        with np.errstate(over="ignore", invalid="ignore"):
            value = loudness(samples, sample_rate)
        report["loudness_lufs"] = value if value is not None and math.isfinite(value) else None
    active = np.flatnonzero(np.abs(samples) > 10 ** (silence_dbfs / 20.0))
    report["active_seconds"] = len(active) / sample_rate
    report["active_span_seconds"] = (int(active[-1] - active[0]) + 1) / sample_rate if len(active) else 0.0
    report["leading_silence_seconds"] = int(active[0]) / sample_rate if len(active) else count / sample_rate
    report["trailing_silence_seconds"] = (count - int(active[-1]) - 1) / sample_rate if len(active) else count / sample_rate
    if not len(active):
        warnings.append({"code": "silent", "message": "No samples exceed the configured silence threshold."})
    step_ratio = float(np.max(np.abs(np.diff(samples / peak)))) if peak and count > 1 else 0.0
    max_step = step_ratio * peak
    if not math.isfinite(max_step):
        max_step = None
    report["edges"] = {"start_amplitude": float(samples[0]), "end_amplitude": float(samples[-1]),
                       "max_step_amplitude": max_step}
    for name, amplitude in (("start", samples[0]), ("end", samples[-1])):
        if abs(amplitude) > 10 ** (edge_dbfs / 20.0):
            warnings.append({"code": f"nonzero_{name}",
                             "message": f"Signal {name} exceeds {edge_dbfs:g} dBFS; an abrupt boundary may click."})
    report["spectral"] = spectrum(samples, sample_rate)
    frame_count = max(1, round(frame_seconds * sample_rate))
    for start in range(0, count, frame_count):
        frame = samples[start:start + frame_count]
        report["frames"].append({"at_seconds": start / sample_rate,
                                 "duration_seconds": len(frame) / sample_rate,
                                 "peak_dbfs": db(float(np.max(np.abs(frame)))),
                                 "rms_dbfs": db(rms(frame)),
                                 "spectral": spectrum(frame, sample_rate)})
    return report
