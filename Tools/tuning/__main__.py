"""Build steps, run from the engine root:

- `python -m Tools.tuning generate <json> <header> <project root>` (CMake `engine_add_tuning`): a tuning header.
- `python -m Tools.tuning input <json> <header> <project root>` (CMake `engine_add_tuning`): an input map header.
- `python -m Tools.tuning settings <Game.settings.json> <out.cpp> <out.rc | -> <project root>` (CMake
  `engine_add_project_settings`): the project settings a game or worker compiles.

Errors go to the build output as `file:line: error: text` and stop the build (exit 1).
"""

import sys
from pathlib import Path

from .document import TuningError
from .header import generate
from . import inputmap, settings


def main(argv: list[str]) -> int:
    if len(argv) == 5 and argv[0] == "settings":
        source = Path(argv[1])
        try:
            settings.generate(source, Path(argv[2]), None if argv[3] == "-" else Path(argv[3]), Path(argv[4]))
        except TuningError as error:
            print(f"{source}:{error.line}: error: {error.text}", file=sys.stderr)
            return 1
        except OSError as error:
            print(f"{source}: error: {error}", file=sys.stderr)
            return 1
        return 0
    if len(argv) != 4 or argv[0] not in ("generate", "input"):
        print("usage: python -m Tools.tuning generate <source.tuning.json> <header.tuning.h> <project root>\n"
              "       python -m Tools.tuning settings <Game.settings.json> <out.cpp> <out.rc | -> <project root>",
              file=sys.stderr)
        return 2
    source, header, root = Path(argv[1]), Path(argv[2]), Path(argv[3])
    try:
        (inputmap.generate if argv[0] == "input" else generate)(source, header, root)
    except TuningError as error:
        print(f"{source}:{error.line}: error: {error.text}", file=sys.stderr)
        return 1
    except OSError as error:
        print(f"{source}: error: {error}", file=sys.stderr)
        return 1
    return 0


sys.exit(main(sys.argv[1:]))
