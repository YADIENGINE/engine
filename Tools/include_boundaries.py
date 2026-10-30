"""Internals/ sınırı denetimi: Docs/Contributing/ApiBoundaries.md'deki üç kademeyi include yollarından doğrular.

- Çekirdek `X/Internals/` (Runtime, Input, Audio, Graphics ...): Engine-private; Engine reposundaki her kod
  (çekirdek, modüller, editör, Engine testleri) include edebilir, oyun/proje kodu edemez.
- `Modules/X/Internals/`: sadece `Modules/X/` (testleri dahil).
- `Editor/Internals/`: sadece Engine `Editor/`.

Sadece engine'in include köklerine göre çözülen ve engine'de var olan `#include "..."` yolları denetlenir; projenin
kendi `Internals/` klasörleri bu denetimin konusu değildir. Komut: `python Engine check` (CTest `Engine.IncludeBoundaries`).
"""

import re
from pathlib import Path

from .manifest import ENGINE_ROOT

CORE = "Core"  # Oyuna giren engine kodu; include kökü
SOURCE_SUFFIXES = {".h", ".hpp", ".c", ".cpp", ".inl"}
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')


def skipped_directory(directory: Path) -> bool:
    """Gizli klasörler, CMake build ağaçları, FetchContent kaynakları ve engine'in ThirdParty/'si taranmaz."""
    return (directory.name.startswith(".") or directory.name in ("_deps", "ThirdParty")
            or (directory / "CMakeCache.txt").exists())


def _sources(directory: Path):
    for entry in sorted(directory.iterdir()):
        if entry.is_dir():
            if not skipped_directory(entry):
                yield from _sources(entry)
        elif entry.suffix in SOURCE_SUFFIXES:
            yield entry


def _owner(include: str, engine_root: Path) -> tuple[Path, str] | None:
    """Internals başlığını include etmeye yetkili klasör ve adı; Engine'in Internals başlığı değilse None.
    Core başlıkları Core'a (`Runtime/...`, `Modules/...`), editörünkiler engine köküne göre çözülür (`Editor/...`)."""
    parts = Path(include).parts
    if "Internals" not in parts:
        return None
    if not any((root / include).is_file() for root in (engine_root / CORE, engine_root)):
        return None
    if parts[0] == "Modules" and len(parts) > 2:
        return engine_root / CORE / "Modules" / parts[1], f"Modules/{parts[1]}"
    if parts[0] == "Editor":
        return engine_root / "Editor", "Editor"
    return engine_root, "Engine"


def violations(project_root: Path, engine_root: Path = ENGINE_ROOT) -> list[str]:
    project_root, engine_root = project_root.resolve(), engine_root.resolve()
    found = []
    for source in _sources(project_root):
        try:
            lines = source.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        for number, line in enumerate(lines, 1):
            match = INCLUDE.match(line)
            if not match:
                continue
            owner = _owner(match.group(1), engine_root)
            if owner is not None and not source.is_relative_to(owner[0]):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: includes "{match.group(1)}" (private to {owner[1]})')
    return found
