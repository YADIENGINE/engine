"""Engine projesini derler, çalıştırır, Release paketini kurar ve testleri koşar.

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


def copy_entry(source: Path, destination: Path) -> None:
    if not source.exists():
        sys.exit(f"Missing package file: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.is_dir():
        shutil.copytree(source, destination)
    else:
        shutil.copy2(source, destination)


def package(project: Project, exe: Path) -> Path:
    """Paket klasörünü (exe + Assets + ek dosyalar) ve yanına ZIP'i kurar. Assets'ten ne gireceğini projenin
    `package_assets(root)` hook'u seçer (Assets'e göre yollar); hook yoksa Assets olduğu gibi kopyalanır."""
    settings = project.section("package")
    extras = [project.path(name) for name in settings.get("files", [])]
    for extra in extras:
        if not extra.is_file():
            sys.exit(f"Missing package file: {extra}")
    target = project.path(settings.get("directory", "Final")) / settings.get("name", project.name)
    print(f"> package {target}", flush=True)
    if target.exists():
        shutil.rmtree(target)
    target.mkdir(parents=True)

    shutil.copy2(exe, target / exe.name)
    for extra in extras:
        shutil.copy2(extra, target / extra.name)
    built_assets = exe.parent / "Assets"
    if built_assets.is_dir():
        select = project.hook("package_assets")
        if select is None:
            shutil.copytree(built_assets, target / "Assets")
        else:
            for name in select(project.root):
                copy_entry(built_assets / name, target / "Assets" / name)

    archive = shutil.make_archive(str(target), "zip", target.parent, target.name)
    print(f"> {archive}", flush=True)
    return target / exe.name


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

    exe = build_dir / config / (game["target"] + (".exe" if sys.platform == "win32" else ""))
    if args.release:
        exe = package(project, exe)

    if args.build:
        return

    print(f"> {exe}", flush=True)
    sys.exit(subprocess.run([str(exe)], cwd=exe.parent).returncode)


if __name__ == "__main__":
    main(Path(__file__).resolve().parents[1])
