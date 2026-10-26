"""EngineEditor'ü ve projenin preview worker'ını derleyip editörü açar.

Proje kökündeki editor.py bu modülün main(root)'unu çağırır; ayarlar Project.json "editor" bölümünden
(bkz. Docs/ProjectSetup.md). Worker'ı ve EngineEditor.project.json tarifini projenin
`engine_add_editor_worker` çağrısı kurar.

Kullanım (editor.py üzerinden):
    python editor.py              Debug derle + editörü aç
    python editor.py --release    Release derle + editörü aç
    python editor.py --build      Sadece derle, editörü açma
    python editor.py --clean      Yalnız editör build klasörünü temizleyip yeniden derle + aç
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

from buildenv import GENERATOR, msvc_env, tool
from project_config import Project

# Editör build'inin her projede sabit seçenekleri; proje kendi seçeneklerini "cacheVariables" ile ekler.
ENGINE_OPTIONS = {
    "CMAKE_CONFIGURATION_TYPES": "Debug;Release",
    "ENGINE_BUILD_RUNTIME": "ON",
    "ENGINE_BUILD_EDITOR": "ON",
    "ENGINE_BUILD_TESTS": "OFF",
    "ENGINE_BUILD_EDITOR_TESTS": "OFF",
}
PROJECT_RECIPE = "EngineEditor.project.json"  # engine_add_editor_worker üretir


def run(command: list[str], env: dict[str, str], directory: Path) -> None:
    print(f"> {subprocess.list2cmdline(command)}", flush=True)
    subprocess.run(command, cwd=directory, env=env, check=True)


def clean(root: Path, build_dir: Path) -> None:
    if not build_dir.exists():
        return
    target = build_dir.resolve()
    # Junction/symlink yoluyla workspace dışına veya başka bir build dizinine silme yapma.
    if target != build_dir or not target.is_relative_to(root):
        sys.exit(f"Refusing to clean redirected editor build directory: {target}")
    print(f"> clean {target}", flush=True)
    shutil.rmtree(target)


def needs_configure(root: Path, build_dir: Path, options: dict[str, str]) -> bool:
    cache_file = build_dir / "CMakeCache.txt"
    if not cache_file.exists():
        return True
    cache = {}
    for line in cache_file.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line or line.startswith(("#", "//")) or "=" not in line:
            continue
        name, value = line.split("=", 1)
        cache[name.split(":", 1)[0]] = value

    if cache.get("CMAKE_GENERATOR") != GENERATOR:
        sys.exit(f"Editor build uses a different generator; rerun with --clean to recreate only {build_dir}")
    source = cache.get("CMAKE_HOME_DIRECTORY")
    if source is None or Path(source).resolve() != root:
        sys.exit("Editor build uses a different source directory; rerun with --clean")
    for compiler in ("CMAKE_C_COMPILER", "CMAKE_CXX_COMPILER"):
        if Path(cache.get(compiler, "")).name.lower() not in ("cl", "cl.exe"):
            sys.exit("Editor build uses a different compiler; rerun with --clean to use MSVC")

    # CMake kaynak değişikliklerini Ninja kendisi izler; scriptin sahip olduğu seçenekleri burada denetle.
    return not (build_dir / "build.ninja").exists() or any(
        cache.get(name) != value for name, value in options.items()
    )


def launch(root: Path) -> None:
    project = Project(root)
    settings = project.section("editor")
    build_dir = project.path(settings.get("buildDirectory", "build/editor"))
    options = ENGINE_OPTIONS | settings.get("cacheVariables", {})

    parser = argparse.ArgumentParser(description="Engine editörünü derle ve aç; MSVC ortamını otomatik hazırlar.")
    parser.add_argument("--release", action="store_true", help="Release modunda derle")
    parser.add_argument("--build", action="store_true", help="Sadece derle, editörü açma")
    parser.add_argument("--clean", action="store_true", help=f"Yalnız {build_dir.relative_to(project.root).as_posix()} klasörünü temizle")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("Engine Editor currently supports Windows desktop only")

    cmake = tool("cmake")
    env = msvc_env(project.msvc_cache)
    if args.clean:
        clean(project.root, build_dir)
    if needs_configure(project.root, build_dir, options):
        command = [cmake, "-S", str(project.root), "-B", str(build_dir), "-G", GENERATOR,
                   "-DCMAKE_C_COMPILER=cl", "-DCMAKE_CXX_COMPILER=cl"]
        command.extend(f"-D{name}={value}" for name, value in options.items())
        run(command, env, project.root)

    config = "Release" if args.release else "Debug"
    run([cmake, "--build", str(build_dir), "--config", config,
         "--target", "EngineEditor", settings["worker"]], env, project.root)
    if args.build:
        return

    executable = build_dir / "Editor" / config / "EngineEditor.exe"
    # Editörün kendi Build & Refresh alt süreçleri de MSVC/Ninja ortamını miras almalı.
    run([str(executable), "--project", str(executable.parent / PROJECT_RECIPE)], env, executable.parent)


def main(root: Path) -> None:
    try:
        launch(root)
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
    except OSError as error:
        sys.exit(f"Editor launcher failed: {error}")
    except KeyboardInterrupt:
        sys.exit(130)
