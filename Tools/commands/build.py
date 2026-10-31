"""`build` ve `run`: projenin oyununu derler, isterse çalıştırır.

Ayarlar Project.json "game" bölümünden (bkz. Docs/ProjectSetup.md).
"""

import subprocess
import sys
from pathlib import Path

from ..cmake import PYTHON_DEFINITION, clear_stale_build, remove_build_dir, run
from ..errors import EXIT_FAILED, EXIT_OK
from ..msvc import msvc_env, tool
from ..project import Project

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
    """Oyunu derler ve exe'nin yolunu döndürür."""
    game = project.section("game")
    config = config_name(release)
    build_dir = project.path(game.get("buildDirectory", "build"))
    if clean:
        remove_build_dir(build_dir, project.root)
    clear_stale_build(build_dir, project.root)

    env = msvc_env(project.msvc_cache)
    if needs_configure(project.root, build_dir):
        run([tool("cmake"), "--preset", game["configurePreset"], PYTHON_DEFINITION], project.root, env)
        (build_dir / CONFIGURE_STAMP).touch()
    run([tool("cmake"), "--build", "--preset", game["buildPresets"][config]], project.root, env)

    target = game.get("target", project.name)
    return build_dir / config / (target + (".exe" if sys.platform == "win32" else ""))


def run_game(exe: Path) -> int:
    """Oyunu kendi klasöründe başlatır ve kapanmasını bekler."""
    print(f"> {exe}", flush=True)
    return EXIT_OK if subprocess.run([str(exe)], cwd=exe.parent).returncode == 0 else EXIT_FAILED
