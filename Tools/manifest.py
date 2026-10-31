"""Engine'in kimliği: EngineManifest.json (ad, sürüm, araç sözleşmesinin sürümü, repo adresi, birikimli notlar).

Alanlar launcher ile sözleşmedir (Docs/ToolsContract.md → Manifest). Notların kuralları `note_problems`'ta; engine
testi gerçek manifest'i onunla denetler, böylece notsuz sürüm artırılamaz.
"""

import json
import re
from dataclasses import dataclass
from pathlib import Path

from .errors import ToolError

ENGINE_ROOT = Path(__file__).resolve().parents[1]
MANIFEST_FILE = "EngineManifest.json"
VERSION_PATTERN = re.compile(r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)")  # ANA.ARA.YAMA
DATE_PATTERN = re.compile(r"\d{4}-\d{2}-\d{2}")


@dataclass(frozen=True)
class Manifest:
    name: str
    version: str
    tools_contract: int


def load_manifest(engine_root: Path = ENGINE_ROOT) -> Manifest:
    path = engine_root / MANIFEST_FILE
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        return Manifest(data["name"], data["version"], data["toolsContract"])
    except (OSError, ValueError, KeyError) as error:
        raise ToolError(f"{path}: {error}") from error


def parse_version(text: str) -> tuple[int, int, int] | None:
    found = VERSION_PATTERN.fullmatch(text) if isinstance(text, str) else None
    return (int(found[1]), int(found[2]), int(found[3])) if found else None


def _strings(value) -> bool:
    return isinstance(value, list) and all(isinstance(item, str) and item.strip() for item in value)


def note_problems(data: dict) -> list[str]:
    """Manifest'in sürüm / not kuralları: `version` notların ilk satırı, sürümler azalan ve tekrarsız, her notta
    `date` (YYYY-MM-DD), `changes` (metin listesi) ve `breaking` ({change, migrate} listesi; kırıcı yoksa boş)."""
    problems = []
    if parse_version(data.get("version")) is None:
        problems.append(f"version '{data.get('version')}' is not MAJOR.MINOR.PATCH")
    repository = data.get("repository")
    if not isinstance(repository, str) or not repository.strip():
        problems.append("repository: the engine repository's address is required")
    notes = data.get("notes")
    if not isinstance(notes, list) or not notes:
        return problems + ["notes: at least the current version's note is required"]

    previous = None
    for index, note in enumerate(notes):
        where = f"notes[{index}]"
        if not isinstance(note, dict):
            problems.append(f"{where}: must be an object")
            continue
        version = parse_version(note.get("version"))
        if version is None:
            problems.append(f"{where}: version '{note.get('version')}' is not MAJOR.MINOR.PATCH")
        elif previous is not None and version >= previous:
            problems.append(f"{where}: versions must be newest first without repeats")
        previous = version or previous
        if not isinstance(note.get("date"), str) or not DATE_PATTERN.fullmatch(note["date"]):
            problems.append(f"{where}: date must be YYYY-MM-DD")
        if not _strings(note.get("changes")):
            problems.append(f"{where}: changes must be a list of texts")
        breaking = note.get("breaking")
        if not isinstance(breaking, list) or not all(
                isinstance(item, dict) and _strings([item.get("change"), item.get("migrate")]) for item in breaking):
            problems.append(f"{where}: breaking must be a list of {{\"change\", \"migrate\"}} (empty when nothing breaks)")
    if notes and isinstance(notes[0], dict) and notes[0].get("version") != data.get("version"):
        problems.append(f"notes[0] is {notes[0].get('version')}; the newest note must be the manifest's version "
                        f"{data.get('version')}")
    return problems
