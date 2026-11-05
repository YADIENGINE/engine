"""Komutların ortak CMake işleri: komut çalıştırma, CMake önbelleğini okuma ve build klasörünü güvenle silme."""

import locale
import shutil
import subprocess
import sys
from pathlib import Path

from .agent_output import quiet, shown, tail, write_log
from .build_errors import summary
from .errors import EXIT_FAILED, ToolError
from .msvc import GENERATOR

BUILD_LOG = "build.log"  # In a build folder: the whole output of its last build; agents see only the summary

# Her configure'a: CMake'in Python adımları (tuning üretimi) araçları çalıştıran yorumlayıcıyı kullanır, PATH'te ya
# da registry'de bulduğu başka bir Python'u değil (launcher'ın gömülü Python'u, ayrıntı Docs/ToolsContract.md).
PYTHON_DEFINITION = f"-DPython3_EXECUTABLE={Path(sys.executable).as_posix()}"


def _text(raw: bytes) -> str:
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:  # MSVC writes the console code page when its output is piped
        return raw.decode(locale.getpreferredencoding(False), errors="replace")


def run(command: list[str], cwd: Path, env: dict[str, str], *, log: Path) -> None:
    """Runs a command and writes its whole output to `log`. A person sees the output as it comes; under an agent
    nothing is printed unless it fails (agent_output.py). A failure prints the summary of the errors in the output
    (build_errors.py) and stops with EXIT_FAILED."""
    agent = quiet()
    line = f"> {subprocess.list2cmdline(command)}"
    if not agent:
        print(line, flush=True)
    output: list[str] = []
    try:
        with subprocess.Popen(command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT) as process:
            for raw in process.stdout:
                text = _text(raw).rstrip("\r\n")
                output.append(text)
                if agent:
                    continue
                if stream := getattr(sys.stdout, "buffer", None):  # The bytes as written: the console decodes them
                    stream.write(raw)
                    stream.flush()
                else:
                    print(text, flush=True)
            code = process.wait()
    except OSError as error:
        raise ToolError(f"Cannot run {command[0]}: {error}") from error
    finally:
        write_log(log, [line, *output])
    if code != 0:
        errors = summary(output, cwd)
        for text in errors or (tail(output) if agent else []):
            print(text, flush=True)
        if agent:
            print(f"Full log: {shown(log)}", flush=True)
        raise ToolError(f"{Path(command[0]).name} exited with code {code}", EXIT_FAILED)


def read_cache(build_dir: Path) -> dict[str, str] | None:
    """CMakeCache.txt'nin `AD:TİP=DEĞER` satırları `AD → DEĞER` olarak; build kurulmamışsa None."""
    cache_file = build_dir / "CMakeCache.txt"
    if not cache_file.exists():
        return None
    cache = {}
    for line in cache_file.read_text(encoding="utf-8", errors="replace").splitlines():
        if line and not line.startswith(("#", "//")) and "=" in line:
            name, value = line.split("=", 1)
            cache[name.split(":", 1)[0]] = value
    return cache


def remove_build_dir(build_dir: Path, root: Path) -> None:
    """Build klasörünü siler. Junction / symlink ile proje dışına yönlenmiş bir klasörü silmeyi reddeder."""
    if not build_dir.exists():
        return
    target = build_dir.resolve()
    if build_dir.is_symlink() or not target.is_relative_to(root.resolve()):
        raise ToolError(f"Refusing to remove a build directory outside the project: {target}")
    print(f"> rm -r {target}", flush=True)
    shutil.rmtree(target)


def clear_stale_build(build_dir: Path, root: Path) -> None:
    """Başka generator'la kurulmuş build klasörünü siler; CMake generator değişimini aynı klasörde kabul etmez."""
    cache = read_cache(build_dir)
    if cache is not None and cache.get("CMAKE_GENERATOR", GENERATOR) != GENERATOR:
        print(f"> {build_dir} uses another generator", flush=True)
        remove_build_dir(build_dir, root)
