"""Internals/ sınırı denetimi: Docs/Contributing/ApiBoundaries.md'deki üç kademeyi include yollarından doğrular.

- Çekirdek `X/Internals/` (Runtime, Input, Audio, Graphics ...): Engine-private; Engine reposundaki her kod
  (çekirdek, modüller, editör, Engine testleri) include edebilir, oyun/proje kodu edemez.
- `Modules/X/Internals/`: sadece `Modules/X/` (testleri dahil).
- `Editor/Internals/`: sadece Engine `Editor/`.

Sadece Engine köküne göre çözülen ve Engine'de var olan `#include "..."` yolları denetlenir; projenin kendi
`Internals/` klasörleri bu denetimin konusu değildir. Kullanım: python Engine/Tools/include_boundaries.py [proje kökü]
"""

import re
import sys
from pathlib import Path

ENGINE_ROOT = Path(__file__).resolve().parents[1]
SOURCE_SUFFIXES = {".h", ".hpp", ".c", ".cpp", ".inl"}
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')


def _skipped(directory: Path) -> bool:
    """Gizli klasörler, CMake build ağaçları ve FetchContent kaynakları taranmaz."""
    return directory.name.startswith(".") or directory.name == "_deps" or (directory / "CMakeCache.txt").exists()


def _sources(directory: Path):
    for entry in sorted(directory.iterdir()):
        if entry.is_dir():
            if not _skipped(entry):
                yield from _sources(entry)
        elif entry.suffix in SOURCE_SUFFIXES:
            yield entry


def _owner(include: str, engine_root: Path) -> Path | None:
    """Internals başlığını include etmeye yetkili klasör; Engine'in Internals başlığı değilse None."""
    parts = Path(include).parts
    if "Internals" not in parts or not (engine_root / include).is_file():
        return None
    if parts[0] == "Modules" and len(parts) > 2:
        return engine_root / "Modules" / parts[1]
    if parts[0] == "Editor":
        return engine_root / "Editor"
    return engine_root


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
            if owner is not None and not source.is_relative_to(owner):
                where = source.relative_to(project_root).as_posix()
                scope = owner.relative_to(engine_root).as_posix() if owner != engine_root else "Engine"
                found.append(f'{where}:{number}: includes "{match.group(1)}" (private to {scope})')
    return found


def main(argv: list[str]) -> int:
    project_root = Path(argv[1]) if len(argv) > 1 else Path.cwd()
    found = violations(project_root)
    for violation in found:
        print(violation)
    print(f"Internals boundaries: {len(found)} violation(s)" if found else "Internals boundaries: OK")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
