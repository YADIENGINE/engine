"""Butterworth, one-pole and time-varying filters, kept as distinct algorithms."""

import numpy as np


def butterworth(samples, cutoff, kind, order, sample_rate):
    from scipy import signal

    sos = signal.butter(order, cutoff, kind, fs=sample_rate, output="sos")
    return signal.sosfilt(sos, samples)


def one_pole(samples, cutoff, sample_rate):
    cutoff = np.broadcast_to(np.asarray(cutoff, dtype=float), samples.shape)
    alpha = 1.0 - np.exp(-2.0 * np.pi * cutoff / sample_rate)
    out = np.empty_like(samples)
    state = 0.0
    for index, value in enumerate(samples):
        state += alpha[index] * (value - state)
        out[index] = state
    return out


def sweep_filter(samples, kind, cutoff, order, width, block_size, sample_rate):
    from scipy import signal

    cutoff = np.broadcast_to(np.asarray(cutoff, dtype=float), samples.shape)
    out = np.empty_like(samples)
    state = None
    for start in range(0, len(samples), block_size):
        end = min(start + block_size, len(samples))
        center = float(np.clip(cutoff[start], 20.0, sample_rate * 0.45))
        if kind == "band":
            half = max(center * width / 2.0, 10.0)
            sos = signal.butter(order, [max(center - half, 15.0), min(center + half, sample_rate * 0.47)],
                                "band", fs=sample_rate, output="sos")
        else:
            sos = signal.butter(order, center, kind, fs=sample_rate, output="sos")
        if state is None:
            state = np.zeros((sos.shape[0], 2))
        out[start:end], state = signal.sosfilt(sos, samples[start:end], zi=state)
    return out


def swept(samples, cutoff_at, kind, order, blocks, sample_rate):
    from scipy import signal

    size = len(samples) // blocks + 1
    window = np.hanning(size * 2)
    result = np.zeros_like(samples)
    for block in range(blocks + 2):
        start = (block - 1) * size
        low, high = max(0, start), min(len(samples), start + size * 2)
        if low >= high:
            continue
        sos = signal.butter(order, cutoff_at((start + size) / sample_rate), kind,
                            fs=sample_rate, output="sos")
        piece = signal.sosfilt(sos, samples[low:high])
        result[low:high] += piece * window[low - start:high - start]
    return result


def sweep_bandpass(samples, start, end, width, blocks, sample_rate):
    seconds = len(samples) / sample_rate

    def cutoff_at(at):
        center = start * (end / start) ** min(max(at / seconds, 0.0), 1.0)
        return center / width, center * width

    return swept(samples, cutoff_at, "bandpass", 2, blocks, sample_rate)


def formant_voice(source, formants, sample_rate):
    out = np.zeros_like(source)
    for center, width, gain in formants:
        out += sweep_filter(source, "band", np.broadcast_to(center, source.shape),
                            2, width, 256, sample_rate) * gain
    return out
