"""Engine'in kimliği: EngineManifest.json (ad, sürüm, araç sözleşmesinin sürümü)."""

import json
from dataclasses import dataclass
from pathlib import Path

from .errors import ToolError

ENGINE_ROOT = Path(__file__).resolve().parents[1]
MANIFEST_FILE = "EngineManifest.json"


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
