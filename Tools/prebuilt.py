"""Which compiled engine a project's build should link (Docs/Prebuilt.md); None means building it from source."""

import hashlib
import json
import os
from pathlib import Path

from .manifest import ENGINE_ROOT
from .project import Project

PREBUILT_VARIABLE = "ENGINE_PREBUILT"  # The launcher's package for this engine (Docs/ToolsContract.md)
COPY_RECORD = "EngineCopy.json"  # Written beside a released engine copy; a checkout has none
STATE_FILE = ".engine-prebuilt.json"  # build/<this>: the package and when the copy last matched its record
SOURCE = "source"  # Project.json "engine": {"build": ...}; anything else takes the package when one is usable


def _state_file(project: Project) -> Path:
    return project.path("build") / STATE_FILE


def _read_state(project: Project) -> dict:
    try:
        return json.loads(_state_file(project).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def write_state(project: Project, state: dict) -> None:
    if not state:
        return
    path = _state_file(project)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(state) + "\n", encoding="utf-8")


def _engine_files(engine: Path) -> list[Path]:
    return [path for path in engine.rglob("*") if path.is_file() and "__pycache__" not in path.parts]


def modified_files(engine: Path, record: dict) -> list[str]:
    """Files that differ from the copy's record: the libraries would not contain what these say."""
    expected = record.get("files", {})
    found = {path.relative_to(engine).as_posix() for path in _engine_files(engine)} - {COPY_RECORD}
    changed = sorted(found ^ set(expected))
    for name in sorted(found & set(expected)):
        digest = hashlib.sha256((engine / name).read_bytes()).hexdigest()
        if f"sha256:{digest}" != expected[name]:
            changed.append(name)
    return changed


def _copy_matches_record(engine: Path, state: dict) -> bool:
    """Hashing takes about a second, so it runs only when a file is newer than the last check that passed."""
    record_file = engine / COPY_RECORD
    if not record_file.is_file():
        return False
    newest = max(path.stat().st_mtime for path in _engine_files(engine))
    if state.get("checked", 0) >= newest:
        return True
    changed = modified_files(engine, json.loads(record_file.read_text(encoding="utf-8")))
    if changed:
        print(f"Engine files differ from the release ({changed[0]}, {len(changed)} in all); "
              "building the engine from source", flush=True)
        return False
    state["checked"] = newest
    return True


def prebuilt_directory(project: Project, engine: Path = ENGINE_ROOT) -> tuple[Path | None, dict]:
    """The package this project may link, and the state to keep. The launcher names the package once; commands run
    without it reuse that choice."""
    state = _read_state(project)
    announced = os.environ.get(PREBUILT_VARIABLE)
    if announced:
        state["directory"] = announced
    directory = state.get("directory")
    if (project.setting("engine", "build") == SOURCE or not directory
            or not (Path(directory) / "Prebuilt.json").is_file() or not _copy_matches_record(engine, state)):
        write_state(project, state)
        return None, state
    write_state(project, state)
    return Path(directory), state


def cache_definition(directory: Path | None) -> str:
    """Always passed, so a build tree follows a change in either direction."""
    return f"-DENGINE_PREBUILT_DIR={directory.as_posix() if directory else ''}"
