"""Files a project's tools leave for whoever shows the project (Docs/ToolsContract.md, Project status files)."""

import json
from datetime import datetime, timezone
from pathlib import Path

LAST_BUILD_FILE = "LastBuild.json"


def write_last_build(build_dir: Path, ok: bool, moment: datetime | None = None) -> None:
    """Records the build's result. Only informs: a failure to write it never fails the build."""
    finished = (moment or datetime.now(timezone.utc)).astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    try:
        build_dir.mkdir(parents=True, exist_ok=True)
        (build_dir / LAST_BUILD_FILE).write_text(json.dumps({"ok": ok, "finished": finished}) + "\n", encoding="utf-8")
    except OSError:
        pass
