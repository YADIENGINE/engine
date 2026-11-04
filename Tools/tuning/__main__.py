"""Build step: `python -m Tools.tuning generate <json> <header> <project root>` (from the engine root; CMake
`engine_add_tuning`).

Errors go to the build output as `file:line: error: text` and stop the build (exit 1).
"""

import sys
from pathlib import Path

from .document import TuningError
from .header import generate


def main(argv: list[str]) -> int:
    if len(argv) != 4 or argv[0] != "generate":
        print("usage: python -m Tools.tuning generate <source.tuning.json> <header.tuning.h> <project root>",
              file=sys.stderr)
        return 2
    source, header, root = Path(argv[1]), Path(argv[2]), Path(argv[3])
    try:
        generate(source, header, root)
    except TuningError as error:
        print(f"{source}:{error.line}: error: {error.text}", file=sys.stderr)
        return 1
    except OSError as error:
        print(f"{source}: error: {error}", file=sys.stderr)
        return 1
    return 0


sys.exit(main(sys.argv[1:]))
