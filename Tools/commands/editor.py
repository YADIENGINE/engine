"""`editor`: EngineEditor'ü ve projenin worker process'ını derleyip editörü açar.

Ayarlar Project.json "editor" bölümünden (bkz. Docs/ProjectSetup.md). Worker'ı ve EngineEditor.project.json
tarifini projenin `engine_add_editor_worker` çağrısı kurar. Editör build'i oyununkinden ayrı bir klasördedir.
"""

import subprocess
import sys
from pathlib import Path

from ..agent_output import quiet, shown
from ..build_lock import build_lock
from ..cmake import BUILD_LOG, PYTHON_DEFINITION, read_cache, remove_build_dir, run
from ..errors import EXIT_FAILED, EXIT_OK, ToolError
from ..msvc import GENERATOR, msvc_env, tool
from ..prebuilt import prebuilt_directory
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
    prebuilt, _ = prebuilt_directory(project)
    options = ENGINE_OPTIONS | settings.get("cacheVariables", {})
    options["ENGINE_PREBUILT_DIR"] = prebuilt.as_posix() if prebuilt else ""

    cmake = tool("cmake")
    env = msvc_env(project.msvc_cache)
    config = config_name(release)
    worker = settings.get("worker", project.name + "EditorWorker")
    with build_lock(build_dir):
        if clean:
            remove_build_dir(build_dir, project.root)
        if needs_configure(project.root, build_dir, options):
            command = [cmake, "-S", str(project.root), "-B", str(build_dir), "-G", GENERATOR,
                       "-DCMAKE_C_COMPILER=cl", "-DCMAKE_CXX_COMPILER=cl", PYTHON_DEFINITION]
            command.extend(f"-D{name}={value}" for name, value in options.items())
            run(command, project.root, env, log=build_dir / BUILD_LOG)
        # CMake decides whether the package fits this project; a package already holds the editor itself.
        packaged = prebuilt is not None and (read_cache(build_dir) or {}).get("ENGINE_PREBUILT_USED") == "TRUE"
        targets = [worker] if packaged else ["EngineEditor", worker]
        run([cmake, "--build", str(build_dir), "--config", config, "--target", *targets], project.root, env,
            log=build_dir / BUILD_LOG)
    if build_only:
        if quiet():
            print(f"Editor build: succeeded (log: {shown(build_dir / BUILD_LOG)})", flush=True)
        return EXIT_OK

    recipe_folder = build_dir / "Editor" / config
    executable = prebuilt / "Editor" / "EngineEditor.exe" if packaged else recipe_folder / "EngineEditor.exe"
    print(f"> {executable}", flush=True)
    # Editörün kendi Build & Refresh alt süreçleri de MSVC / Ninja ortamını miras alır.
    code = subprocess.run([str(executable), "--project", str(recipe_folder / PROJECT_RECIPE)],
                          cwd=recipe_folder, env=env).returncode
    return EXIT_OK if code == 0 else EXIT_FAILED
