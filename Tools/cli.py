"""`python Engine <komut>`: engine araçlarının tek giriş noktası.

Komutlar, seçenekler, JSON çıktıları ve çıkış kodları launcher ile sözleşmedir (Docs/ToolsContract.md). Sözleşmeyi
bozan her değişiklik EngineManifest.json "toolsContract" sürümünü artırır; yeni komut / seçenek eklemek bozmaz.
"""

import argparse
import json
import os
import sys
from pathlib import Path

from .commands.build import build, run_game
from .commands.editor import editor
from .commands.new import new
from .commands.refresh import refresh
from .commands.release import package
from .commands.test import run_tests
from .errors import EXIT_FAILED, EXIT_OK, ToolError
from .include_boundaries import violations
from .manifest import ENGINE_ROOT, load_manifest
from .project import PROJECT_FILE, Project
from .tuning.check import check_tuning

INTERRUPTED = 130  # Ctrl+C; kabukların ortak kodu


def default_root() -> Path:
    """Varsayılan proje kökü: engine bir projenin içindeyse üst klasör, değilse (engine reposu) engine'in kendisi."""
    parent = ENGINE_ROOT.parent
    return parent if (parent / PROJECT_FILE).is_file() else ENGINE_ROOT


def entry_command(root: Path) -> list[str]:
    """Bu aracı proje kökünden başlatan komut; test raporlarının "yeniden koş" satırı için."""
    return ["python", Path(os.path.relpath(ENGINE_ROOT, root)).as_posix()]


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="python Engine", description="Engine tools: build, run, test and package "
                                     "a project. Contract: Engine/Docs/ToolsContract.md")
    parser.add_argument("--project", type=Path, help="project root (default: the folder above Engine/ when it has a "
                        f"{PROJECT_FILE}, otherwise the engine itself)")
    commands = parser.add_subparsers(dest="command", required=True, metavar="<command>")

    command = commands.add_parser("build", help="build the game")
    command.add_argument("--release", action="store_true", help="Release configuration")
    command.add_argument("--clean", action="store_true", help="delete the game's build folder first")

    command = commands.add_parser("run", help="build and run the game")
    command.add_argument("--release", action="store_true", help="Release configuration")

    commands.add_parser("release", help="Release build, then the package folder and ZIP")

    command = commands.add_parser("test", help="build and run the headless tests")
    command.add_argument("--filter", help="CTest test-name regular expression")
    command.add_argument("--label", help="CTest label regular expression (subsystem)")
    command.add_argument("--list", action="store_true", help="list the selected tests without running them")
    command.add_argument("--clean", action="store_true", help="delete the test build folder first")

    command = commands.add_parser("editor", help="build the editor and the project's previews, then open it")
    command.add_argument("--release", action="store_true", help="Release configuration")
    command.add_argument("--build-only", action="store_true", help="build without opening the editor")
    command.add_argument("--clean", action="store_true", help="delete the editor's build folder first")

    commands.add_parser("refresh", help="Build & Refresh in the project's open editor; the preview stays live")

    command = commands.add_parser("new", help="set up the folder above Engine/ as a new project")
    command.add_argument("--name", help="project name (asked when omitted)")
    command.add_argument("--dry-run", action="store_true", help="list what would be created without writing")

    commands.add_parser("info", help="engine and project as JSON")
    command = commands.add_parser("check", help="check the Internals/ include boundaries and the tuning files")
    command.add_argument("--fix", action="store_true", help="rewrite valid tuning files in the canonical layout")
    return parser


def describe(root: Path) -> dict:
    manifest = load_manifest()
    project = Project(root) if (root / PROJECT_FILE).is_file() else None
    return {
        "engine": {"name": manifest.name, "version": manifest.version, "root": str(ENGINE_ROOT)},
        "toolsContract": manifest.tools_contract,
        "project": {"name": project.name, "root": str(project.root)} if project is not None else None,
    }


def dispatch(args: argparse.Namespace) -> int:
    root = (args.project or default_root()).resolve()
    match args.command:
        case "build":
            build(Project(root), args.release, args.clean)
            return EXIT_OK
        case "run":
            return run_game(build(Project(root), args.release, clean=False))
        case "release":
            project = Project(root)
            package(project, build(project, release=True, clean=False))
            return EXIT_OK
        case "test":
            project = Project(root)
            return run_tests(project.root, project.section("tests"), entry_command(project.root) + ["test"],
                             project.msvc_cache, args.filter, args.list, args.clean, label=args.label)
        case "editor":
            return editor(Project(root), args.release, args.build_only, args.clean)
        case "refresh":
            return refresh(Project(root))
        case "new":
            return new(ENGINE_ROOT, args.project.resolve() if args.project else ENGINE_ROOT.parent, args.name,
                       args.dry_run)
        case "info":
            print(json.dumps(describe(root), indent=2))
            return EXIT_OK
        case "check":
            found = violations(root)
            for violation in found:
                print(violation)
            print(f"Internals boundaries: {len(found)} violation(s)" if found else "Internals boundaries: OK")
            problems, fixed = check_tuning(root, args.fix)
            for line in problems + [f"{path}: rewritten in the canonical layout" for path in fixed]:
                print(line)
            print(f"Tuning files: {len(problems)} problem(s)" if problems else "Tuning files: OK")
            return EXIT_FAILED if found or problems else EXIT_OK
    raise AssertionError(f"unhandled command {args.command}")


def main(argv: list[str]) -> int:
    args = build_parser().parse_args(argv)  # Kullanım hatasında argparse EXIT_INFRA (2) ile çıkar
    try:
        return dispatch(args)
    except ToolError as error:
        print(f"error: {error}", file=sys.stderr)
        return error.exit_code
    except KeyboardInterrupt:
        return INTERRUPTED
