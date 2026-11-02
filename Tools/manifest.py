"""Engine'in kimliği: EngineManifest.json (ad, sürüm, araç sözleşmesinin sürümü, repo adresi) ve ReleaseNotes/<sürüm>.json.

İkisi de launcher ile sözleşmedir (Docs/ToolsContract.md → Manifest). Notların kuralları `note_problems`'ta; engine
testi gerçek dosyaları onunla denetler, böylece notsuz sürüm artırılamaz.
"""

import json
import re
from dataclasses import dataclass
from pathlib import Path

from .errors import ToolError

ENGINE_ROOT = Path(__file__).resolve().parents[1]
MANIFEST_FILE = "EngineManifest.json"
NOTES_FOLDER = "ReleaseNotes"
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


def load_notes(engine_root: Path = ENGINE_ROOT) -> list[dict]:
    """Her ReleaseNotes/<sürüm>.json, en yeni sürüm önce; `version` dosya adından gelir. Adı sürüm olmayan dosya sona
    düşer ve `note_problems` onu bildirir."""
    notes = []
    for path in (engine_root / NOTES_FOLDER).glob("*.json"):
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            raise ToolError(f"{path}: {error}") from error
        notes.append({**(data if isinstance(data, dict) else {}), "version": path.stem})
    return sorted(notes, key=lambda note: parse_version(note["version"]) or (-1, -1, -1), reverse=True)


def note_problems(data: dict, notes: list[dict]) -> list[str]:
    """Manifest'in ve notların kuralları (`notes`: `load_notes` sırasıyla): `version` en yeni notun sürümü, her notta
    `date` (YYYY-MM-DD), `changes` (metin listesi) ve `breaking` ({change, migrate} listesi; kırıcı yoksa boş)."""
    problems = []
    if "notes" in data:
        problems.append(f"notes: release notes live in {NOTES_FOLDER}/<version>.json, not in the manifest")
    if parse_version(data.get("version")) is None:
        problems.append(f"version '{data.get('version')}' is not MAJOR.MINOR.PATCH")
    repository = data.get("repository")
    if not isinstance(repository, str) or not repository.strip():
        problems.append("repository: the engine repository's address is required")
    if not notes:
        return problems + [f"{NOTES_FOLDER}/{data.get('version')}.json: the current version's note is required"]

    for note in notes:
        where = f"{NOTES_FOLDER}/{note.get('version')}.json"
        if parse_version(note.get("version")) is None:
            problems.append(f"{where}: the file name is not MAJOR.MINOR.PATCH")
        if not isinstance(note.get("date"), str) or not DATE_PATTERN.fullmatch(note["date"]):
            problems.append(f"{where}: date must be YYYY-MM-DD")
        if not _strings(note.get("changes")):
            problems.append(f"{where}: changes must be a list of texts")
        breaking = note.get("breaking")
        if not isinstance(breaking, list) or not all(
                isinstance(item, dict) and _strings([item.get("change"), item.get("migrate")]) for item in breaking):
            problems.append(f"{where}: breaking must be a list of {{\"change\", \"migrate\"}} (empty when nothing breaks)")
    if notes[0].get("version") != data.get("version"):
        problems.append(f"The newest note is {notes[0].get('version')}; it must be the manifest's version "
                        f"{data.get('version')} ({NOTES_FOLDER}/{data.get('version')}.json)")
    return problems
