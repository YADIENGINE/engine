"""Internals/ sınırı denetimi: Docs/Contributing/ApiBoundaries.md'deki üç kademeyi include yollarından doğrular.

- Çekirdek `X/Internals/` (Runtime, Input, Audio, Graphics ...): Engine-private; Engine reposundaki her kod
  (çekirdek, modüller, editör, Engine testleri) include edebilir, oyun/proje kodu edemez.
- `Modules/X/Internals/`: sadece `Modules/X/` (testleri dahil).
- `Editor/Internals/`, `Terminal/Internals/`: sadece Engine `Editor/` / `Terminal/`.

Sadece engine'in include köklerine göre çözülen ve engine'de var olan `#include "..."` yolları denetlenir; projenin
kendi `Internals/` klasörleri bu denetimin konusu değildir. Komut: `python Engine check` (CTest `Engine.IncludeBoundaries`).

Simulation code (the engine's `Core/Simulation` and a project's simulation folder) also stays away from everything that
would make a replay differ or tie it to a window: drawing, audio, scenes, the runtime, clocks and standard random
numbers. The build enforces the same at link time; this names the mistake earlier (Docs/Simulation.md).
"""

import re
from pathlib import Path

from .manifest import ENGINE_ROOT, MANIFEST_FILE

CORE = "Core"  # Oyuna giren engine kodu; include kökü
SOURCE_SUFFIXES = {".h", ".hpp", ".c", ".cpp", ".inl"}
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"')
CONSUMER_INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
SIMULATION_FORBIDDEN = re.compile(
    r'''^\s*#\s*include\s*(?:"(?:Graphics|Audio|Fx|Scene|Runtime|GameObjects|Input|Ui|Options|Platform|Editor'''
    r'''|Terminal|ToolUi|Physics/Debug)/[^"]*"'''
    r'''|[<"](?:[^>"/]*/)*(?:raylib|raymath|rlgl)\.h[>"]|<(?:random|chrono|ctime|time\.h|thread|future|mutex)>)''')
PROJECT_SIMULATION_FOLDER = "Game/Simulation"
# The UI's layers, lowest first (Docs/Ui.md): what each folder under Core/Ui must not include. The kernel knows no
# widget, so that a game's widget is as good as the engine's; widgets do not draw, looks do not read input.
UI_LAYER_RULES = (
    ("Layout", re.compile(r'''^\s*#\s*include\s*"(?!Utilities/|Ui/Layout/)[^"]*"'''), "only Utilities"),
    ("Kernel", re.compile(r'''^\s*#\s*include\s*"Ui/(?:Looks|Widgets|Kit)/[^"]*"'''), "no widgets or looks"),
    ("Looks", re.compile(r'''^\s*#\s*include\s*"(?:Ui/(?:Widgets|Kit)|Input)/[^"]*"'''), "no widgets or input"),
    ("Widgets", re.compile(r'''^\s*#\s*include\s*"(?:Ui/Kit|Graphics/(?:Paint|Draw|Style|Surface))/[^"]*"'''),
     "no drawing: that is the look's"),
)
PLATFORM_INCLUDE = re.compile(r'^\s*#\s*include\s*[<"](?:[^>"/]*/)*(raylib|raymath|rlgl)\.h[>"]')


def skipped_directory(directory: Path, engine_root: Path = ENGINE_ROOT) -> bool:
    """Gizli klasörler, CMake build ağaçları, FetchContent kaynakları, engine'in ThirdParty/'si ve projenin içindeki
    başka engine kopyaları (kendi EngineManifest.json'u olan klasör; ör. projede duran bir launcher reposunun
    engine'leri) taranmaz: onlar projenin kodu değildir."""
    return (directory.name.startswith(".") or directory.name in ("_deps", "ThirdParty")
            or (directory / "CMakeCache.txt").exists()
            or ((directory / MANIFEST_FILE).is_file() and directory.resolve() != engine_root.resolve()))


def _sources(directory: Path, engine_root: Path):
    for entry in sorted(directory.iterdir()):
        if entry.is_dir():
            if not skipped_directory(entry, engine_root):
                yield from _sources(entry, engine_root)
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
    if parts[0] in ("Editor", "Terminal"):
        return engine_root / parts[0], parts[0]
    return engine_root, "Engine"


def _simulation_folders(project_root: Path, engine_root: Path) -> list[Path]:
    project = project_root / PROJECT_SIMULATION_FOLDER
    return [engine_root / CORE / "Simulation", engine_root / CORE / "Tests" / "Simulation", project]


def violations(project_root: Path, engine_root: Path = ENGINE_ROOT) -> list[str]:
    project_root, engine_root = project_root.resolve(), engine_root.resolve()
    simulation = _simulation_folders(project_root, engine_root)
    found = []
    for source in _sources(project_root, engine_root):
        try:
            lines = source.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        recipe = source.is_relative_to(engine_root / "AgentDocs" / "Recipes" / "Code")
        rules = any(source.is_relative_to(folder) for folder in simulation)
        ui_layer = next((rule for rule in UI_LAYER_RULES
                         if source.is_relative_to(engine_root / CORE / "Ui" / rule[0])), None)
        for number, line in enumerate(lines, 1):
            if ui_layer and ui_layer[1].match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: Core/Ui/{ui_layer[0]} includes "{line.strip()}"; {ui_layer[2]}')
            if rules and SIMULATION_FORBIDDEN.match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: simulation code includes "{line.strip()}"; see Docs/Simulation.md')
            if recipe and PLATFORM_INCLUDE.match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: recipe includes a platform-only header')
            match = (CONSUMER_INCLUDE if recipe else INCLUDE).match(line)
            if not match:
                continue
            owner = _owner(match.group(1), engine_root)
            if owner is not None and (recipe or not source.is_relative_to(owner[0])):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: includes "{match.group(1)}" (private to {owner[1]})')
    return found
