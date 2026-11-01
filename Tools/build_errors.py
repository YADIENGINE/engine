"""Build errors out of compiler / linker / CMake output, as `file:line:column: CODE message` lines.

Failed `build`, `test`, `editor` and `refresh` runs print this summary after their output, so a reader (often an agent)
does not have to search hundreds of lines for the cause. Recognizes MSVC, clang / gcc, the MSVC linker, CMake errors and
`cmake -E` failures; repeats are dropped.
"""

import re
from dataclasses import dataclass
from pathlib import Path

MAX_SHOWN = 20

COMPILER = re.compile(
    r"^\s*(?P<file>.+?)\((?P<line>\d+)(?:,(?P<column>\d+))?\)\s*:\s*(?:fatal\s+)?error\s+(?P<code>[A-Z]+\d+)\s*:\s*"
    r"(?P<text>.*)$")
GNU = re.compile(r"^\s*(?P<file>.+?):(?P<line>\d+):(?P<column>\d+):\s*(?:fatal\s+)?error:\s*(?P<text>.*)$")
LINKER = re.compile(r"^\s*(?P<file>.+?)\s*:\s*(?:fatal\s+)?error\s+(?P<code>LNK\d+)\s*:\s*(?P<text>.*)$")
CMAKE = re.compile(r"^CMake Error(?: at (?P<file>.+?):(?P<line>\d+))?")
CMAKE_TOOL = re.compile(r"^Error (?:copying|removing|creating|renaming) .*$")


@dataclass(frozen=True)
class BuildError:
    file: str  # Relative to the project when inside it; empty when the error has no location
    line: int
    column: int
    code: str
    text: str

    def __str__(self) -> str:
        where = self.file
        if where and self.line:
            where += f":{self.line}" + (f":{self.column}" if self.column else "")
        parts = [part for part in (self.code, self.text) if part]
        return f"{where}: {' '.join(parts)}" if where else " ".join(parts)


def _location(file: str, root: Path | None) -> str:
    path = Path(file.strip())
    if root is not None and path.is_absolute():
        try:
            return path.resolve().relative_to(root.resolve()).as_posix()
        except (ValueError, OSError):
            pass
    return path.as_posix()


def build_errors(lines: list[str], root: Path | None = None) -> list[BuildError]:
    """The distinct errors in `lines`, in first-seen order."""
    found: list[BuildError] = []
    seen: set[BuildError] = set()

    def add(error: BuildError) -> None:
        if error not in seen:
            seen.add(error)
            found.append(error)

    for index, raw in enumerate(lines):
        line = raw.rstrip()
        if match := COMPILER.match(line):
            add(BuildError(_location(match["file"], root), int(match["line"]), int(match["column"] or 0),
                           match["code"], match["text"].strip()))
        elif match := LINKER.match(line):
            add(BuildError(_location(match["file"], root), 0, 0, match["code"], match["text"].strip()))
        elif match := GNU.match(line):
            add(BuildError(_location(match["file"], root), int(match["line"]), int(match["column"]), "",
                           match["text"].strip()))
        elif match := CMAKE.match(line):
            detail = next((text.strip() for text in lines[index + 1:index + 4] if text.strip()), "")
            add(BuildError(_location(match["file"], root) if match["file"] else "", int(match["line"] or 0), 0,
                           "CMake", detail))
        elif CMAKE_TOOL.match(line):
            add(BuildError("", 0, 0, "", line.strip()))
    return found


def summary(lines: list[str], root: Path | None = None) -> list[str]:
    """Lines to print after a failed run; empty when no error was recognized."""
    errors = build_errors(lines, root)
    if not errors:
        return []
    shown = [f"  {error}" for error in errors[:MAX_SHOWN]]
    if len(errors) > MAX_SHOWN:
        shown.append(f"  ... and {len(errors) - MAX_SHOWN} more")
    return [f"Errors ({len(errors)}):", *shown]
