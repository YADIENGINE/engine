"""`build` ve `run`: projenin oyununu derler, isterse çalıştırır.

Ayarlar Project.json "game" bölümünden (bkz. Docs/ProjectSetup.md).
"""

import sys
from pathlib import Path

from ..agent_output import quiet, shown
from ..build_lock import build_lock
from ..cmake import BUILD_LOG, PYTHON_DEFINITION, clear_stale_build, read_cache, remove_build_dir, run
from ..errors import EXIT_FAILED, EXIT_OK
from ..crash_reports import run_native
from ..msvc import msvc_env, tool
from ..prebuilt import cache_definition, prebuilt_directory, write_state
from ..project import Project
from ..project_status import write_last_build

CONFIGURE_STAMP = ".configured"  # Son başarılı configure; bkz. needs_configure


def config_name(release: bool) -> str:
    return "Release" if release else "Debug"


def needs_configure(root: Path, build_dir: Path) -> bool:
    """Kurulu build için configure gereksiz: build.ninja, CMake dosyaları değişince CMake'i her build'de kendisi
    yeniden çalıştırır. Preset değişikliklerini yakalamadığı için CMakePresets.json son başarılı configure'un
    damgasıyla karşılaştırılır (CMakeCache.txt değişiklik yoksa yeniden yazılmaz, tarihi kullanılamaz)."""
    stamp = build_dir / CONFIGURE_STAMP
    if not (build_dir / "CMakeCache.txt").exists() or not (build_dir / "build.ninja").exists() or not stamp.exists():
        return True
    return (root / "CMakePresets.json").stat().st_mtime > stamp.stat().st_mtime


def build(project: Project, release: bool, clean: bool) -> Path:
    """Compiles the game and records the result for a launcher (Docs/ToolsContract.md, Project status files)."""
    build_dir = project.path(project.section("game").get("buildDirectory", "build"))
    try:
        with build_lock(build_dir):
            built = _compile(project, release, clean)
    except BaseException as failure:
        if not isinstance(failure, KeyboardInterrupt):
            write_last_build(build_dir, False)
        raise
    write_last_build(build_dir, True)
    if quiet():  # Nothing else was printed
        print(f"Build: succeeded (log: {shown(build_dir / BUILD_LOG)})", flush=True)
    return built


def _compile(project: Project, release: bool, clean: bool) -> Path:
    """Oyunu derler ve exe'nin yolunu döndürür."""
    game = project.section("game")
    config = config_name(release)
    build_dir = project.path(game.get("buildDirectory", "build"))
    prebuilt, state = prebuilt_directory(project)
    if clean:
        remove_build_dir(build_dir, project.root)
    clear_stale_build(build_dir, project.root)
    write_state(project, state)  # The folder that holds it may be the one just removed

    env = msvc_env(project.msvc_cache)
    definition = cache_definition(prebuilt)
    cached = (read_cache(build_dir) or {}).get("ENGINE_PREBUILT_DIR", "")
    if needs_configure(project.root, build_dir) or cached != definition.split("=", 1)[1]:
        run([tool("cmake"), "--preset", game["configurePreset"], PYTHON_DEFINITION, definition], project.root, env,
            log=build_dir / BUILD_LOG)
        (build_dir / CONFIGURE_STAMP).touch()
    run([tool("cmake"), "--build", "--preset", game["buildPresets"][config]], project.root, env,
        log=build_dir / BUILD_LOG)

    target = game.get("target", project.name)
    return build_dir / config / (target + (".exe" if sys.platform == "win32" else ""))


def run_game(exe: Path) -> int:
    """Oyunu kendi klasöründe başlatır ve kapanmasını bekler."""
    print(f"> {exe}", flush=True)
    exe = exe.resolve()
    return EXIT_OK if run_native(exe, [], exe.parent.parent / "crashes", cwd=exe.parent) == 0 else EXIT_FAILED
