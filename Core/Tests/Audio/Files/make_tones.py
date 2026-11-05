"""Writes the short sounds of the audio format tests: 441 Hz, exactly one second at 44.1 kHz, mono, amplitude 0.5.

Run by hand when the files must change; the tests use the files. Needs numpy and soundfile.
"""

from pathlib import Path

import numpy
import soundfile

RATE = 44100
tone = 0.5 * numpy.sin(2.0 * numpy.pi * 441.0 * numpy.arange(RATE) / RATE)
folder = Path(__file__).resolve().parent
soundfile.write(folder / "tone.ogg", tone, RATE, format="OGG", subtype="VORBIS")
soundfile.write(folder / "tone.mp3", tone, RATE, format="MP3", subtype="MPEG_LAYER_III")
soundfile.write(folder / "tone.flac", tone, RATE, format="FLAC", subtype="PCM_16")
