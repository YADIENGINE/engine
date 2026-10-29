"""Generate a self-contained synthetic reference review; no game content required."""

import argparse
from pathlib import Path
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from AudioGeneration import SoundGenerator


def impact(context, frequency, decay, air_gain):
    body = context.for_layer("body")
    air = context.for_layer("air")
    t = body.time_axis(0.7)
    tone = (np.sin(2 * np.pi * frequency * t) + 0.3 * np.sin(2 * np.pi * frequency * 2.7 * t))
    tone *= body.decay_envelope(t, 0.001, decay)
    noise = air.bandpass(air.noise(0.2), 800, 3000) * np.exp(-air.time_axis(0.2) / 0.025)
    return context.review_clip(layers={"Body": body.layer(tone, at=0.03, gain=0.55),
                                      "Air": air.layer(noise, at=0.03, gain=air_gain)},
                               parameters={"frequency_hz": frequency, "decay_seconds": decay, "air_gain": air_gain})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("AudioReview"))
    args = parser.parse_args()
    g = SoundGenerator(24000, seed=42)
    reference = impact(g.for_sound("Reference"), 140, 0.10, 0.15)
    variants = {"Heavy / short": impact(g.for_sound("Heavy"), 80, 0.08, 0.1),
                "Bright / ringing": impact(g.for_sound("Bright"), 300, 0.22, 0.3),
                "Close / dry": impact(g.for_sound("Close"), 150, 0.09, 0.12)}
    paths = g.export_review(args.output, reference, variants, title="Impact studies", target_dbfs=-24)
    print(paths["html"].resolve())


if __name__ == "__main__":
    main()
