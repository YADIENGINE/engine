"""Engine klasörünün bir üstünü yeni bir proje olarak kurar (EngineSetup.py çağırır; bkz. Docs/ProjectSetup.md).

ProjectTemplate/ olduğu gibi proje köküne yazılır; `@PROJECT_NAME@` proje adıyla değişir, `gitignore` /
`gitattributes` noktalı adını alır (Engine reposunun kendi git ayarlarını etkilemesinler diye noktasız saklanır).
Var olan hiçbir dosyanın üzerine yazılmaz; kökte Project.json varsa proje zaten kurulmuş sayılır.
"""

import argparse
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

from buildenv import visual_studio
from project_config import PROJECT_FILE

ENGINE_FOLDER = "Engine"  # Proje CMake'i, sarmalayıcılar ve Android Gradle bu adı bekler
TEMPLATE = Path(__file__).resolve().parent / "ProjectTemplate"
PLACEHOLDER = "@PROJECT_NAME@"
RENAMES = {"gitignore": ".gitignore", "gitattributes": ".gitattributes"}
NAME_PATTERN = re.compile(r"[A-Za-z][A-Za-z0-9_]{0,63}")  # CMake hedefi ve exe adı olur
MIN_PYTHON = (3, 10)
MIN_CMAKE = (3, 24)


@dataclass
class SetupResult:
    root: Path
    created: list[Path] = field(default_factory=list)
    skipped: list[Path] = field(default_factory=list)


def name_error(name: str) -> str | None:
    if not NAME_PATTERN.fullmatch(name):
        return "use a letter followed by letters, digits or '_' (at most 64 characters, no spaces)"
    return None


def default_name(root: Path) -> str:
    """Kök klasörün adından geçerli bir proje adı; çıkarılamazsa "MyGame"."""
    candidate = re.sub(r"[^A-Za-z0-9_]", "", root.name)
    return candidate if name_error(candidate) is None else "MyGame"


def project_root(engine: Path) -> Path:
    if engine.name != ENGINE_FOLDER:
        sys.exit(f"This folder is named '{engine.name}'. Rename it to '{ENGINE_FOLDER}' (projects include it by that "
                 f"name; a GitHub zip extracts as '<repo>-main') and run EngineSetup.py again.")
    return engine.parent


def template_files() -> list[tuple[Path, Path]]:
    """(şablondaki dosya, proje köküne göre hedef) çiftleri."""
    files = []
    for source in sorted(TEMPLATE.rglob("*")):
        if source.is_file() and "__pycache__" not in source.parts:
            relative = source.relative_to(TEMPLATE)
            files.append((source, relative.with_name(RENAMES.get(relative.name, relative.name))))
    return files


def setup_project(engine: Path, name: str, dry_run: bool = False) -> SetupResult:
    root = project_root(engine)
    if (error := name_error(name)) is not None:
        sys.exit(f"Invalid project name '{name}': {error}")
    if (root / PROJECT_FILE).exists():
        sys.exit(f"{root / PROJECT_FILE} already exists; this project is already set up")

    result = SetupResult(root)
    for source, relative in template_files():
        destination = root / relative
        if destination.exists():
            result.skipped.append(relative)
            continue
        result.created.append(relative)
        if dry_run:
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        content = source.read_bytes()
        destination.write_bytes(content.replace(PLACEHOLDER.encode(), name.encode()))
    return result


def environment_problems() -> list[str]:
    """Build'den önce eksik araçlar; boş liste her şey hazır demektir."""
    problems = []
    if sys.version_info < MIN_PYTHON:
        problems.append(f"Python {MIN_PYTHON[0]}.{MIN_PYTHON[1]}+ is required (this is {sys.version.split()[0]})")
    cmake = shutil.which("cmake")
    if cmake is None:
        problems.append(f"CMake {MIN_CMAKE[0]}.{MIN_CMAKE[1]}+ not found on PATH")
    else:
        output = subprocess.run([cmake, "--version"], capture_output=True, text=True).stdout
        found = re.search(r"(\d+)\.(\d+)", output)
        if found and (int(found[1]), int(found[2])) < MIN_CMAKE:
            problems.append(f"CMake {found[0]} is too old; {MIN_CMAKE[0]}.{MIN_CMAKE[1]}+ is required")
    if sys.platform == "win32":
        install = visual_studio()
        if install is None:
            problems.append("Visual Studio 2022 (Build Tools) with the C++ workload not found")
        elif not (install / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "Ninja").is_dir():
            problems.append("Ninja not found; add the 'C++ CMake tools for Windows' Visual Studio component")
    return problems


def ask_name(root: Path) -> str:
    suggestion = default_name(root)
    while True:
        try:
            name = input(f"Project name [{suggestion}]: ").strip() or suggestion
        except EOFError:
            sys.exit("\nNo project name given")
        if (error := name_error(name)) is None:
            return name
        print(f"  Invalid name: {error}")


def main(engine: Path) -> None:
    parser = argparse.ArgumentParser(description="Set up the folder above Engine/ as a new project.")
    parser.add_argument("--dry-run", action="store_true", help="List what would be created without writing")
    args = parser.parse_args()

    root = project_root(engine)
    if (root / PROJECT_FILE).exists():
        sys.exit(f"{root / PROJECT_FILE} already exists; this project is already set up")
    print(f"Project folder: {root}")
    name = ask_name(root)
    result = setup_project(engine, name, args.dry_run)

    verb = "Would create" if args.dry_run else "Created"
    for relative in result.created:
        print(f"  {verb} {relative.as_posix()}")
    for relative in result.skipped:
        print(f"  Kept existing {relative.as_posix()}")
    if args.dry_run:
        return

    problems = environment_problems()
    if problems:
        print("\nInstall before building:")
        for problem in problems:
            print(f"  - {problem}")
    print(f"""
'{name}' is ready. From {root}:
  python p.py          build and run (an empty Game/ shows the engine starter screen)
  python editor.py     EngineEditor with this project's previews
  python p.py --test   headless tests
First game: Engine/Docs/GettingStarted.md, previews: Engine/Docs/Editor/GettingStarted.md""")
