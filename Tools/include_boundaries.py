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
    r'''^\s*#\s*include\s*(?:"(?:Graphics|Audio|Fx|Scene|Stage|Runtime|Input|Ui|Options|Platform|Editor'''
    r'''|Terminal|ToolUi|Physics/Debug|Camera/Debug)/[^"]*"'''
    r'''|[<"](?:[^>"/]*/)*(?:raylib|raymath|rlgl)\.h[>"]|<(?:random|chrono|ctime|time\.h|thread|future|mutex)>)''')
PROJECT_SIMULATION_FOLDER = "Game/Simulation"
# The UI's layers, lowest first (Docs/Ui.md): what each folder under Core/Ui must not include. The kernel knows no
# widget, so that a game's widget is as good as the engine's; widgets do not draw, looks do not read input.
UI_LAYER_RULES = (
    ("Layout", re.compile(r'''^\s*#\s*include\s*"(?!Utilities/|Ui/Layout/)[^"]*"'''), "only Utilities"),
    ("Kernel", re.compile(r'''^\s*#\s*include\s*"(?:Ui/(?:Looks|Widgets|Kit|Sound)|Audio)/[^"]*"'''),
     "no widgets, looks or audio"),
    ("Looks", re.compile(r'''^\s*#\s*include\s*"(?:Ui/(?:Widgets|Kit|Sound)|Input|Audio)/[^"]*"'''),
     "no widgets, input or audio"),
    ("Widgets",
     re.compile(r'''^\s*#\s*include\s*"(?:Ui/(?:Kit|Sound)|Audio|Graphics/(?:Paint|Draw|Style|Surface))/[^"]*"'''),
     "no drawing or audio: widgets say what happened"),
    ("Sound", re.compile(r'''^\s*#\s*include\s*"(?!Ui/(?:Kernel|Sound)/|Audio/)[^"]*"'''), "only the kernel and audio"),
)
# Core/Camera computes and draws nothing, so a simulation may keep a camera that decides gameplay; only its debug guides
# draw (Docs/Camera.md).
CAMERA_RULE = re.compile(r'''^\s*#\s*include\s*"(?!Utilities/|Animation/|Camera/)[^"]*"''')
# Core/Procedural generates layouts from plain data, so a simulation can build its level with it (Docs/Procedural.md).
PROCEDURAL_RULE = re.compile(r'''^\s*#\s*include\s*"(?!Utilities/|Procedural/)[^"]*"''')
# Lighting stands on Painter, like a game's own drawing, and Painter does not know it. One file bridges colliders to it.
LIGHT_BRIDGE = "CollisionOccluders"
LIGHT_RULE = re.compile(
    r'''^\s*#\s*include\s*"(?!Utilities/|Camera/CameraView\.h|Graphics/(?:Light|Paint|Surface)/)[^"]*"''')
# The air stands on Painter as light does, and reads weather as plain data. Weather itself draws nothing, so a simulation
# may keep it.
ATMOSPHERE_RULE = re.compile(
    r'''^\s*#\s*include\s*"(?!Utilities/|Camera/CameraView\.h|Weather/|Graphics/(?:Atmosphere|Draw/ParticleDraw|Paint)[/.])[^"]*"''')
WEATHER_RULE = re.compile(r'''^\s*#\s*include\s*"(?!Utilities/|Animation/|Weather/)[^"]*"''')
# A stage assembles view systems (cameras, effects, light, air, listeners) and borrows the scene's services. It is view
# code, so it never reaches a simulation, collision, input or the runtime (Docs/Graphics/Stage.md).
STAGE_RULE = re.compile(
    r'''^\s*#\s*include\s*"(?!Utilities/|Animation/|Camera/|Fx/|Graphics/|Weather/|Audio/Sound(?:Player|Space)\.h|Scene/SceneServices\.h|Stage/)[^"]*"''')
# The pixel kit is plain data over Utilities, so any drawing code can make sprites; only its Internals (the GPU atlas)
# reach the device. Painter draws sprites through it (Docs/Graphics/PixelArt.md).
PIXEL_RULE = re.compile(r'''^\s*#\s*include\s*"(?!Utilities/|Graphics/Pixel/)[^"]*"''')
PIXEL_INTERNALS_RULE = re.compile(
    r'''^\s*#\s*include\s*"(?!Utilities/|Graphics/Pixel/|Graphics/Gpu/|Graphics/Text/Internals/AtlasPacker\.h)[^"]*"''')
PAINT_LIGHT_RULE = re.compile(r'''^\s*#\s*include\s*"Graphics/(?:Light|PixelView)/[^"]*"''')
# A pixel view stands on Painter as light does: a surface, the camera's view, and Painter never knows it
# (Docs/Graphics/PixelArt.md).
PIXEL_VIEW_RULE = re.compile(
    r'''^\s*#\s*include\s*"(?!Utilities/|Camera/CameraView\.h|Graphics/(?:PixelView|Paint|Surface)/)[^"]*"''')
# The audio library stays behind Core/Audio/Internals, so that public audio headers read the same whatever plays them, and
# no audio code touches raylib. What a public audio header includes stays low enough for a tool to use it (Docs/Audio.md).
AUDIO_LIBRARY_INCLUDE = re.compile(r'''^\s*#\s*include\s*[<"](?:[^>"/]*/)*(?:miniaudio\.h|stb_vorbis\.c)[>"]''')
AUDIO_PUBLIC_RULE = re.compile(r'''^\s*#\s*include\s*"(?!Utilities/|Animation/|Audio/)[^"]*"''')
# PFR is the one vendored library a public header includes; only the engine's own facade names it, so the rest of the
# engine and every game see plain-struct reflection in engine words (Docs/Simulation.md#state).
REFLECTION_LIBRARY_INCLUDE = re.compile(r'''^\s*#\s*include\s*[<"](?:pfr\.hpp|pfr/[^>"]*)[>"]''')
# Text shaping stays behind Graphics/Text/Internals: FontFace and TextBlock are the API (Docs/Graphics/Text.md).
SHAPING_LIBRARY_INCLUDE = re.compile(r'''^\s*#\s*include\s*[<"](?:[^>"/]*/)*kb_text_shape\.h[>"]''')
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
        camera = (source.is_relative_to(engine_root / CORE / "Camera")
                  and not source.is_relative_to(engine_root / CORE / "Camera" / "Debug"))
        procedural = source.is_relative_to(engine_root / CORE / "Procedural")
        light = source.is_relative_to(engine_root / CORE / "Graphics" / "Light") and source.stem != LIGHT_BRIDGE
        paint = source.is_relative_to(engine_root / CORE / "Graphics" / "Paint")
        pixel = source.is_relative_to(engine_root / CORE / "Graphics" / "Pixel")
        pixel_internal = source.is_relative_to(engine_root / CORE / "Graphics" / "Pixel" / "Internals")
        pixel_view = source.is_relative_to(engine_root / CORE / "Graphics" / "PixelView")
        atmosphere = source.is_relative_to(engine_root / CORE / "Graphics" / "Atmosphere")
        weather = source.is_relative_to(engine_root / CORE / "Weather")
        stage = source.is_relative_to(engine_root / CORE / "Stage")
        audio = source.is_relative_to(engine_root / CORE / "Audio")
        audio_internal = source.is_relative_to(engine_root / CORE / "Audio" / "Internals")
        engine_code = source.is_relative_to(engine_root)
        reflection_facade = source == engine_root / CORE / "Utilities" / "Reflection.h"
        text_internal = source.is_relative_to(engine_root / CORE / "Graphics" / "Text" / "Internals")
        for number, line in enumerate(lines, 1):
            if atmosphere and ATMOSPHERE_RULE.match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: Core/Graphics/Atmosphere includes "{line.strip()}"; only Utilities, the'
                             ' camera\'s view, Weather, Graphics/Paint and particle shapes')
            if stage and STAGE_RULE.match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: Core/Stage includes "{line.strip()}"; only view systems (Utilities,'
                             ' Animation, Camera, Fx, Graphics, Weather), the sound player and SceneServices')
            if weather and WEATHER_RULE.match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: Core/Weather includes "{line.strip()}"; only Utilities and Animation,'
                             ' so a simulation can use it')
            if engine_code and not audio_internal and AUDIO_LIBRARY_INCLUDE.match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: includes "{line.strip()}"; the audio library stays in'
                             ' Core/Audio/Internals')
            if not reflection_facade and REFLECTION_LIBRARY_INCLUDE.match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: includes "{line.strip()}"; use Utilities/Reflection.h, the only file'
                             ' that includes PFR')
            if not text_internal and SHAPING_LIBRARY_INCLUDE.match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: includes "{line.strip()}"; text shaping stays in'
                             ' Core/Graphics/Text/Internals')
            if audio and PLATFORM_INCLUDE.match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: Core/Audio includes "{line.strip()}"; audio does not use raylib')
            if audio and not audio_internal and AUDIO_PUBLIC_RULE.match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: Core/Audio includes "{line.strip()}"; a public audio header stands on'
                             ' Utilities, Animation and Audio')
            if light and LIGHT_RULE.match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: Core/Graphics/Light includes "{line.strip()}"; only Utilities, the'
                             ' camera\'s view and Graphics/Paint or Surface')
            if pixel and (PIXEL_INTERNALS_RULE if pixel_internal else PIXEL_RULE).match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: Core/Graphics/Pixel includes "{line.strip()}"; the pixel kit stands on'
                             ' Utilities (its Internals also on Graphics/Gpu and the atlas packer)')
            if pixel_view and PIXEL_VIEW_RULE.match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: Core/Graphics/PixelView includes "{line.strip()}"; only Utilities,'
                             ' the camera\'s view and Graphics/Paint or Surface')
            if paint and PAINT_LIGHT_RULE.match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: Core/Graphics/Paint includes "{line.strip()}"; lighting and pixel'
                             ' views stand on Painter, not the other way')
            if ui_layer and ui_layer[1].match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: Core/Ui/{ui_layer[0]} includes "{line.strip()}"; {ui_layer[2]}')
            if camera and CAMERA_RULE.match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: Core/Camera includes "{line.strip()}"; only Utilities, Animation and'
                             ' Camera, so a simulation can use it')
            if procedural and PROCEDURAL_RULE.match(line):
                where = source.relative_to(project_root).as_posix()
                found.append(f'{where}:{number}: Core/Procedural includes "{line.strip()}"; only Utilities and'
                             ' Procedural, so a simulation can use it')
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
