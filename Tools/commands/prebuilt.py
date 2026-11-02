"""`prebuilt`: compiles this engine once in Release into a package that projects link (Docs/Prebuilt.md)."""

import shutil
from pathlib import Path

from ..cmake import PYTHON_DEFINITION, run
from ..errors import EXIT_OK, ToolError
from ..manifest import ENGINE_ROOT
from ..msvc import GENERATOR, msvc_env, tool

BUILD_FOLDER = "build/prebuilt"
RUNTIME = "MultiThreadedDLL"  # Core/Prebuilt.cmake ENGINE_PREBUILT_RUNTIME


def prebuilt(output: Path) -> int:
    build_dir = ENGINE_ROOT / BUILD_FOLDER
    env = msvc_env(ENGINE_ROOT / "build" / ".msvc-env.json")
    cmake = tool("cmake")
    output = output.resolve()
    if output.is_relative_to(build_dir):
        raise ToolError(f"The package cannot live in the build folder it comes from: {output}")
    shutil.rmtree(output, ignore_errors=True)
    output.mkdir(parents=True)
    run([cmake, "-S", str(ENGINE_ROOT), "-B", str(build_dir), "-G", GENERATOR, "-DCMAKE_C_COMPILER=cl",
         "-DCMAKE_CXX_COMPILER=cl", "-DCMAKE_CONFIGURATION_TYPES=Release", f"-DCMAKE_MSVC_RUNTIME_LIBRARY={RUNTIME}",
         "-DENGINE_BUILD_RUNTIME=ON", "-DENGINE_BUILD_EDITOR=ON", "-DENGINE_BUILD_TESTS=OFF",
         "-DENGINE_BUILD_EDITOR_TESTS=OFF", f"-DENGINE_PREBUILT_OUTPUT={output.as_posix()}", PYTHON_DEFINITION],
        ENGINE_ROOT, env)
    run([cmake, "--build", str(build_dir), "--config", "Release", "--target", "EnginePrebuiltPackage"], ENGINE_ROOT, env)
    print(f"Prebuilt engine: {output}", flush=True)
    return EXIT_OK
