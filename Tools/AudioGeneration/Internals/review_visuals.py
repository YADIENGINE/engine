"""Bounded waveform, envelope and log-frequency STFT data for offline HTML."""

import numpy as np


def plots(samples, rate):
    count = len(samples)
    if not count:
        return {"waveform": [], "envelope": [], "spectrogram": [], "frequencies": []}
    bins = min(600, count)
    edges = np.linspace(0, count, bins + 1, dtype=int)
    waveform, envelope = [], []
    for start, end in zip(edges[:-1], edges[1:]):
        block = samples[start:end]
        waveform.append([float(block.min()), float(block.max())])
        envelope.append(float(np.sqrt(np.mean(block ** 2))))
    size = min(2048, max(32, 2 ** int(np.ceil(np.log2(min(max(count, 1), 2048))))))
    window = np.hanning(size)
    frequencies = np.geomspace(min(20, rate / 8), rate / 2, 96)
    fft_frequencies = np.fft.rfftfreq(size, 1 / rate)
    spectrogram = []
    centers = np.linspace(0, max(0, count - 1), min(180, max(1, int(np.ceil(count / (size / 4))))), dtype=int)
    for center in centers:
        start = int(center) - size // 2
        block = np.zeros(size)
        low, high = max(0, start), min(count, start + size)
        block[low - start:high - start] = samples[low:high]
        magnitude = np.abs(np.fft.rfft(block * window)) / (window.sum() / 2)
        values = np.interp(frequencies, fft_frequencies, magnitude)
        dbfs = 20 * np.log10(np.maximum(values, 1e-6))
        spectrogram.append(np.round(np.clip(dbfs, -120, 24), 1).tolist())
    return {"waveform": waveform, "envelope": envelope, "spectrogram": spectrogram,
            "frequencies": frequencies.tolist()}
