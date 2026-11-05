"""Short output when a coding agent runs a command.

Everything a command prints stays in the agent's context and is read again on every later request, so under an agent
`build`, `test`, `sim`, `editor --build-only` and `refresh` print only their result and, on failure, the `Errors (N):`
summary; the full output goes to a log file whose path the result names. Claude Code sets CLAUDECODE and Codex sets
CODEX_CI; ENGINE_AGENT=1 forces the short output and ENGINE_AGENT=0 turns it off.
"""

import os
from pathlib import Path

AGENT_VARIABLES = ("CLAUDECODE", "CODEX_CI")
FORCE = "ENGINE_AGENT"
TAIL_LINES = 30  # Shown from the log when a failure has no recognizable error line
DIAGNOSTIC_LINES = 30  # A failed test's diagnostics shown before pointing at the report

_written: set[Path] = set()  # Logs this process has started: the first write replaces, later ones append


def quiet() -> bool:
    forced = os.environ.get(FORCE, "").strip()
    if forced:
        return forced != "0"
    return any(os.environ.get(name, "").strip() for name in AGENT_VARIABLES)


def write_log(path: Path, lines: list[str]) -> None:
    """Appends `lines` to the command's log; a log is started fresh by the first write of each command run."""
    path.parent.mkdir(parents=True, exist_ok=True)
    resolved = path.resolve()
    with path.open("a" if resolved in _written else "w", encoding="utf-8", newline="\n") as log:
        log.writelines(line + "\n" for line in lines)
    _written.add(resolved)


def shown(path: Path) -> str:
    """A log path as printed: relative to the folder the command runs in when inside it, so it stays short."""
    try:
        return path.resolve().relative_to(Path.cwd().resolve()).as_posix()
    except (ValueError, OSError):
        return str(path)


def tail(lines: list[str], count: int = TAIL_LINES) -> list[str]:
    """The last lines of an output, said to be the last ones when some were left out."""
    if len(lines) <= count:
        return list(lines)
    return [f"... last {count} of {len(lines)} lines:", *lines[-count:]]


def shorten(text: str, count: int = DIAGNOSTIC_LINES) -> str:
    """The first `count` lines of a text, with how many were left out."""
    lines = text.splitlines()
    if len(lines) <= count:
        return text
    return "\n".join(lines[:count] + [f"... {len(lines) - count} more lines"])
