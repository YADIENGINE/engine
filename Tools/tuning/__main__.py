"""Derleme adımı: `python -m Tools.tuning generate <json> <header>` (engine kökünden; CMake `engine_add_tuning`).

Hata derleme çıktısına `dosya:satır: error: açıklama` olarak yazılır ve derleme durur (çıkış 1).
"""

import sys
from pathlib import Path

from .document import TuningError
from .header import generate


def main(argv: list[str]) -> int:
    if len(argv) != 3 or argv[0] != "generate":
        print("usage: python -m Tools.tuning generate <source.tuning.json> <header.tuning.h>", file=sys.stderr)
        return 2
    source, header = Path(argv[1]), Path(argv[2])
    try:
        generate(source, header)
    except TuningError as error:
        print(f"{source}:{error.line}: error: {error.text}", file=sys.stderr)
        return 1
    except OSError as error:
        print(f"{source}: error: {error}", file=sys.stderr)
        return 1
    return 0


sys.exit(main(sys.argv[1:]))
