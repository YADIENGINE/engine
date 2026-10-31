"""`editor`: EngineEditor'ü ve projenin preview worker'ını derleyip editörü açar.

Ayarlar Project.json "editor" bölümünden (bkz. Docs/ProjectSetup.md). Worker'ı ve EngineEditor.project.json
tarifini projenin `engine_add_editor_worker` çağrısı kurar. Editör build'i oyununkinden ayrı bir klasördedir.
"""

import subprocess
import sys
from pathlib import Path

from ..cmake import PYTHON_DEFINITION, read_cache, remove_build_dir, run
from ..errors import EXIT_FAILED, EXIT_OK, ToolError
from ..msvc import GENERATOR, msvc_env, tool
from ..project import Project
from .build import config_name

# Editör build'inin her projede sabit seçenekleri; proje kendi seçeneklerini "cacheVariables" ile ekler.
ENGINE_OPTIONS = {
    "CMAKE_CONFIGURATION_TYPES": "Debug;Release",
    "ENGINE_BUILD_RUNTIME": "ON",
    "ENGINE_BUILD_EDITOR": "ON",
    "ENGINE_BUILD_TESTS": "OFF",
    "ENGINE_BUILD_EDITOR_TESTS": "OFF",
}
PROJECT_RECIPE = "EngineEditor.project.json"  # engine_add_editor_worker üretir


def needs_configure(root: Path, build_dir: Path, options: dict[str, str]) -> bool:
    """Kurulu editör build'inin bu projeye ve MSVC'ye ait olduğunu denetler; araca ait seçenekler değiştiyse
    yeniden configure ister. CMake kaynak değişikliklerini Ninja kendisi izler."""
    cache = read_cache(build_dir)
    if cache is None:
        return True
    if cache.get("CMAKE_GENERATOR") != GENERATOR:
        raise ToolError(f"Editor build uses a different generator; rerun with --clean to recreate only {build_dir}")
    source = cache.get("CMAKE_HOME_DIRECTORY")
    if source is None or Path(source).resolve() != root:
        raise ToolError("Editor build uses a different source directory; rerun with --clean")
    for compiler in ("CMAKE_C_COMPILER", "CMAKE_CXX_COMPILER"):
        if Path(cache.get(compiler, "")).name.lower() not in ("cl", "cl.exe"):
            raise ToolError("Editor build uses a different compiler; rerun with --clean to use MSVC")
    return not (build_dir / "build.ninja").exists() or any(cache.get(name) != value for name, value in options.items())


def editor_build_dir(project: Project) -> Path:
    """The editor's own build directory; the running editor also announces its control endpoint there."""
    return project.path(project.section("editor").get("buildDirectory", "build/editor"))


def editor(project: Project, release: bool, build_only: bool, clean: bool) -> int:
    if sys.platform != "win32":
        raise ToolError("The engine editor currently supports Windows desktop only")
    settings = project.section("editor")
    build_dir = editor_build_dir(project)
    options = ENGINE_OPTIONS | settings.get("cacheVariables", {})

    cmake = tool("cmake")
    env = msvc_env(project.msvc_cache)
    if clean:
        remove_build_dir(build_dir, project.root)
    if needs_configure(project.root, build_dir, options):
        command = [cmake, "-S", str(project.root), "-B", str(build_dir), "-G", GENERATOR,
                   "-DCMAKE_C_COMPILER=cl", "-DCMAKE_CXX_COMPILER=cl", PYTHON_DEFINITION]
        command.extend(f"-D{name}={value}" for name, value in options.items())
        run(command, project.root, env)

    config = config_name(release)
    worker = settings.get("worker", project.name + "Preview")
    run([cmake, "--build", str(build_dir), "--config", config, "--target", "EngineEditor", worker], project.root, env)
    if build_only:
        return EXIT_OK

    executable = build_dir / "Editor" / config / "EngineEditor.exe"
    print(f"> {executable}", flush=True)
    # Editörün kendi Build & Refresh alt süreçleri de MSVC / Ninja ortamını miras alır.
    code = subprocess.run([str(executable), "--project", str(executable.parent / PROJECT_RECIPE)],
                          cwd=executable.parent, env=env).returncode
    return EXIT_OK if code == 0 else EXIT_FAILED
