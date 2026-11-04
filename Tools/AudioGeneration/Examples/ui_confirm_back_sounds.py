"""The engine's menu confirm and back sounds (Assets/Audio/Ui): the same soft struck note, rising and falling.

Run: python -B Tools/AudioGeneration/Examples/ui_confirm_back_sounds.py [Confirm Back] [--check] [--gallery x.html] [--output DIR]
Direction carries the meaning: a rising fifth goes forward, a falling fourth goes back and sits lower and quieter.
Without --output this rewrites the engine's files.
"""

from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from AudioGeneration import SoundGenerator

OUT = Path(__file__).resolve().parents[3] / "Assets" / "Audio" / "Ui"
PACK = SoundGenerator(44100).pack(OUT, prefix="Ui", fade_out=0.008, trim=None)


def struck(g, note, seconds, decay):
    """A small mallet on a bar: a round fundamental with a faint, quickly gone overtone."""
    return g.modal(seconds, g.note(note), (1.0, 3.93), (decay, decay * 0.2), gains=(1.0, 0.16), attack=0.002,
                   random_phase=False)


@PACK.sound("Confirm", peak=0.55)
def confirm(g):
    return g.mix([(0.0, struck(g, "E5", 0.09, 0.022)), (0.055, struck(g, "B5", 0.125, 0.034))])


@PACK.sound("Back", peak=0.45)
def back(g):
    tone = g.mix([(0.0, struck(g, "B4", 0.08, 0.018), 0.8), (0.05, struck(g, "F#4", 0.11, 0.03))])
    return g.lowpass(tone, 2600.0)


if __name__ == "__main__":
    PACK.main()
