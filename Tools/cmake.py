"""Komutların ortak CMake işleri: komut çalıştırma, CMake önbelleğini okuma ve build klasörünü güvenle silme."""

import shutil
import subprocess
import sys
from pathlib import Path

from .errors import EXIT_FAILED, ToolError
from .msvc import GENERATOR

# Her configure'a: CMake'in Python adımları (tuning üretimi) araçları çalıştıran yorumlayıcıyı kullanır, PATH'te ya
# da registry'de bulduğu başka bir Python'u değil (launcher'ın gömülü Python'u, ayrıntı Docs/ToolsContract.md).
PYTHON_DEFINITION = f"-DPython3_EXECUTABLE={Path(sys.executable).as_posix()}"


def run(command: list[str], cwd: Path, env: dict[str, str]) -> None:
    """Komutu çıktısı terminale akarak çalıştırır; başarısızsa EXIT_FAILED ile durur."""
    print(f"> {subprocess.list2cmdline(command)}", flush=True)
    try:
        code = subprocess.run(command, cwd=cwd, env=env).returncode
    except OSError as error:
        raise ToolError(f"Cannot run {command[0]}: {error}") from error
    if code != 0:
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
