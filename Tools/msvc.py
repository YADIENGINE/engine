"""Ninja derlemesi için MSVC ortamı ve kullanıcının araçları (cmake, ctest).

Ninja cl.exe / link.exe / rc.exe'yi ve INCLUDE / LIB yollarını ortamdan bekler; bunları vcvars64.bat kurar.
vcvars her çağrıda 1-2 sn sürdüğü için yakalanan ortam build/ altında saklanır ve VS güncellenince yenilenir.
"""

import json
import os
import shutil
import subprocess
from pathlib import Path

from .errors import ToolError

GENERATOR = "Ninja Multi-Config"  # CMakePresets.json ile aynı; başka generator'la kurulmuş build'i tanımak için
VSWHERE = (Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
           / "Microsoft Visual Studio" / "Installer" / "vswhere.exe")
NINJA_COMPONENT = "'C++ CMake tools for Windows'"  # Ninja'yı getiren Visual Studio bileşeni


def visual_studio() -> Path | None:
    """C++ x64 araçları kurulu en yeni Visual Studio'nun kök klasörü; yoksa None."""
    if not VSWHERE.exists():
        return None
    query = [str(VSWHERE), "-latest", "-products", "*", "-requires",
             "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"]
    install = subprocess.run(query, capture_output=True, text=True).stdout.strip()
    return Path(install) if install else None


def _vcvars() -> Path:
    if not VSWHERE.exists():
        raise ToolError("vswhere.exe not found; install Visual Studio 2022 Build Tools (C++ workload)")
    install = visual_studio()
    vcvars = install / "VC" / "Auxiliary" / "Build" / "vcvars64.bat" if install else None
    if vcvars is None or not vcvars.exists():
        raise ToolError("No Visual Studio installation with the C++ x64 toolset found")
    return vcvars


def _capture(vcvars: Path) -> dict[str, str]:
    # cmd /c ""...vcvars64.bat" >nul && set": vcvars'ın kurduğu ortamın tamamı
    result = subprocess.run(f'"{vcvars}" >nul && set', shell=True, capture_output=True, text=True,
                            encoding="oem", errors="replace")
    if result.returncode != 0:
        raise ToolError(f"vcvars64.bat failed:\n{result.stdout}{result.stderr}")
    after = {key.upper(): value for key, value in
             (line.split("=", 1) for line in result.stdout.splitlines() if "=" in line and not line.startswith("="))}
    if "VCTOOLSINSTALLDIR" not in after:
        raise ToolError("vcvars64.bat did not set up the MSVC environment")
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
        raise ToolError(f"ninja.exe not found; install the {NINJA_COMPONENT} component of Visual Studio")
    return merged


def tool(name: str) -> str:
    """cmake / ctest'in kullanıcı PATH'indeki tam yolu (VS'nin paketlediği eski sürüm değil)."""
    path = shutil.which(name)
    if path is None:
        raise ToolError(f"{name} not found on PATH; install CMake >= 3.24")
    return path
