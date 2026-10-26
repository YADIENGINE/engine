"""Ninja derlemesi için MSVC ortamı (p.py, editor.py ve tests/runner.py ortak kullanır).

Ninja cl.exe / link.exe / rc.exe'yi ve INCLUDE / LIB yollarını ortamdan bekler; bunları vcvars64.bat kurar.
vcvars her çağrıda 1-2 sn sürdüğü için yakalanan ortam build/ altında saklanır ve VS güncellenince yenilenir.
"""

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

GENERATOR = "Ninja Multi-Config"  # CMakePresets.json ile aynı; eski VS build klasörünü tanımak için
_VSWHERE = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"


def visual_studio() -> Path | None:
    """C++ x64 araçları kurulu en yeni Visual Studio'nun kök klasörü; yoksa None."""
    if not _VSWHERE.exists():
        return None
    query = [str(_VSWHERE), "-latest", "-products", "*", "-requires",
             "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"]
    install = subprocess.run(query, capture_output=True, text=True).stdout.strip()
    return Path(install) if install else None


def _vcvars() -> Path:
    if not _VSWHERE.exists():
        sys.exit("vswhere.exe not found; install Visual Studio 2022 Build Tools (C++ workload)")
    install = visual_studio()
    vcvars = install / "VC" / "Auxiliary" / "Build" / "vcvars64.bat" if install else None
    if vcvars is None or not vcvars.exists():
        sys.exit("No Visual Studio installation with the C++ x64 toolset found")
    return vcvars


def _capture(vcvars: Path) -> dict[str, str]:
    # cmd /c ""...vcvars64.bat" >nul && set": vcvars'ın kurduğu ortamın tamamı
    result = subprocess.run(f'"{vcvars}" >nul && set', shell=True, capture_output=True, text=True,
                            encoding="oem", errors="replace")
    if result.returncode != 0:
        sys.exit(f"vcvars64.bat failed:\n{result.stdout}{result.stderr}")
    after = {key.upper(): value for key, value in
             (line.split("=", 1) for line in result.stdout.splitlines() if "=" in line and not line.startswith("="))}
    if "VCTOOLSINSTALLDIR" not in after:
        sys.exit("vcvars64.bat did not set up the MSVC environment")
    # Sadece vcvars'ın eklediği/değiştirdiği değerler saklanır; o anki terminalin PATH'i önbelleğe girmez
    before = {key.upper(): value for key, value in os.environ.items()}
    known = {entry.lower() for entry in before.get("PATH", "").split(os.pathsep)}
    added = {key: value for key, value in after.items() if key != "PATH" and before.get(key) != value}
    added["PATH"] = os.pathsep.join(entry for entry in after.get("PATH", "").split(os.pathsep)
                                    if entry and entry.lower() not in known)
    return added


def msvc_env(cache: Path) -> dict[str, str]:
    """Derleme komutlarına verilecek ortam. vcvars yolları önde: Git'in usr/bin/link.exe'si MSVC link'ini
    gölgelemesin. cmake/ctest VS'nin eski kopyası yerine kullanıcınınki olsun diye `tool` ile tam yoldan çağrılır."""
    env: dict[str, str] | None = None
    if cache.exists():
        try:
            env = json.loads(cache.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            env = None
        if env is not None and not Path(env.get("VCTOOLSINSTALLDIR", "")).exists():
            env = None  # VS güncellendi; eski araç sürümünün klasörü kalktı
    if env is None:
        env = _capture(_vcvars())
        cache.parent.mkdir(parents=True, exist_ok=True)
        cache.write_text(json.dumps(env), encoding="utf-8")

    merged = {key.upper(): value for key, value in os.environ.items()}
    merged.update(env)
    merged["PATH"] = env.get("PATH", "") + os.pathsep + os.environ.get("PATH", "")
    if shutil.which("ninja", path=merged["PATH"]) is None:
        sys.exit("ninja.exe not found; install the 'C++ CMake tools for Windows' component of Visual Studio")
    return merged


def tool(name: str) -> str:
    """cmake / ctest'in kullanıcı PATH'indeki tam yolu (VS'nin paketlediği eski sürüm değil)."""
    path = shutil.which(name)
    if path is None:
        sys.exit(f"{name} not found on PATH; install CMake >= 3.24")
    return path


def clear_stale_build(build_dir: Path) -> None:
    """Başka generator'la kurulmuş build klasörünü siler; CMake generator değişimini aynı klasörde kabul etmez."""
    cache = build_dir / "CMakeCache.txt"
    if not cache.exists():
        return
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("CMAKE_GENERATOR:INTERNAL="):
            if line.split("=", 1)[1] != GENERATOR:
                print(f"> rm -r {build_dir} (generator changed)", flush=True)
                shutil.rmtree(build_dir)
            return
