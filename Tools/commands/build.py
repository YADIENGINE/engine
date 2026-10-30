"""Engine projesini derler, çalıştırır, Release paketini kurar (ReleaseSystem/release.py) ve testleri koşar.

Proje kökündeki p.py bu modülün main(root)'unu çağırır; ayarlar Project.json'dan (bkz. Docs/ProjectSetup.md).
Engine reposunda doğrudan `python Tools/build_cli.py --test` Engine'in kendi testlerini koşar.

Kullanım (p.py üzerinden):
    python p.py              Debug derle + çalıştır
    python p.py --release    Release derle, paketi kur + çalıştır
    python p.py --build      Sadece derle, çalıştırma
    python p.py --clean      Oyun build klasörünü silip sıfırdan derle
    python p.py --test       Headless testleri derle + koş (--filter, --label, --list)
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

from buildenv import clear_stale_build, msvc_env, tool
from project_config import Project
from test_runner import run_tests

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "ReleaseSystem"))
from release import package  # noqa: E402

CONFIGURE_STAMP = ".configured"  # Son başarılı configure; bkz. needs_configure


def run(cmd: list[str], root: Path, env: dict[str, str]) -> None:
    print(f"> {' '.join(cmd)}", flush=True)
    result = subprocess.run(cmd, cwd=root, env=env)
    if result.returncode != 0:
        sys.exit(result.returncode)


def needs_configure(root: Path, build_dir: Path) -> bool:
    """Kurulu build için configure gereksiz: build.ninja, CMake dosyaları değişince CMake'i her build'de kendisi
    yeniden çalıştırır. Preset değişikliklerini yakalamadığı için CMakePresets.json son başarılı
    configure'un damgasıyla karşılaştırılır (CMakeCache.txt değişiklik yoksa yeniden yazılmaz, tarihi kullanılamaz)."""
    stamp = build_dir / CONFIGURE_STAMP
    if not (build_dir / "CMakeCache.txt").exists() or not (build_dir / "build.ninja").exists():
        return True
    if not stamp.exists():
        return True
    return (root / "CMakePresets.json").stat().st_mtime > stamp.stat().st_mtime


def entry_command(root: Path) -> list[str]:
    """Testlerin "yeniden koş" komutu için bu aracı başlatan script, proje köküne göre."""
    script = Path(sys.argv[0]).resolve()
    name = script.relative_to(root).as_posix() if script.is_relative_to(root) else script.name
    return ["python", name]


def main(root: Path) -> None:
    project = Project(root)
    parser = argparse.ArgumentParser(description=f"{project.name}'i derle ve çalıştır.")
    parser.add_argument("--release", action="store_true", help="Release modunda derle ve paketle")
    parser.add_argument("--build", action="store_true", help="Sadece derle, çalıştırma")
    parser.add_argument("--clean", action="store_true", help="Build klasörünü silip sıfırdan derle")
    parser.add_argument("--test", action="store_true", help="Build and run isolated headless Debug tests")
    parser.add_argument("--filter", help="CTest test-name regular expression (requires --test)")
    parser.add_argument("--label", help="CTest subsystem label regular expression (requires --test)")
    parser.add_argument("--list", action="store_true", help="List matching tests (requires --test)")
    args = parser.parse_args()

    if args.test:
        if args.release or args.build:
            parser.error("--test cannot be combined with --release or --build")
        sys.exit(run_tests(project.root, project.section("tests"), entry_command(project.root), project.msvc_cache,
                           args.filter, args.list, args.clean, label=args.label))
    if args.filter is not None or args.label is not None or args.list:
        parser.error("--filter, --label and --list require --test")

    game = project.section("game")
    config = "Release" if args.release else "Debug"
    build_dir = project.path(game.get("buildDirectory", "build"))

    if args.clean and build_dir.exists():
        print(f"> rm -r {build_dir}", flush=True)
        shutil.rmtree(build_dir)

    clear_stale_build(build_dir)
    env = msvc_env(project.msvc_cache)
    if needs_configure(project.root, build_dir):
        run([tool("cmake"), "--preset", game["configurePreset"]], project.root, env)
        (build_dir / CONFIGURE_STAMP).touch()

    run([tool("cmake"), "--build", "--preset", game["buildPresets"][config]], project.root, env)

    target = game.get("target", project.name)
    exe = build_dir / config / (target + (".exe" if sys.platform == "win32" else ""))
    if args.release:
        exe = package(project, exe)

    if args.build:
        return

    print(f"> {exe}", flush=True)
    sys.exit(subprocess.run([str(exe)], cwd=exe.parent).returncode)


if __name__ == "__main__":
    main(Path(__file__).resolve().parents[1])
