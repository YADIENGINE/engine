"""`asset`: the project's graphic assets on disk (Game/Assets/Graphics). The editor's Assets panel runs these too.

`new` writes a graphic's skeleton, `mkdir` adds a folder, and `move` moves an asset or folder and updates the literal
`#include "Assets/Graphics/..."` lines across the project. Engine assets are read-only and out of reach here.
"""

import argparse
import json
import re
from pathlib import Path, PurePosixPath

from ..errors import EXIT_FAILED, EXIT_OK, ToolError
from ..project import Project

GRAPHICS = Path("Game") / "Assets" / "Graphics"
INCLUDE_ROOT = "Assets/Graphics"
NAME = re.compile(r"[A-Z][A-Za-z0-9]*")
SOURCE_SUFFIXES = {".h", ".hpp", ".cpp", ".inl"}
SKIPPED_FOLDERS = {"build", "Engine", "Launcher", "Release", ".git"}
INCLUDE = re.compile(r'(#\s*include\s*")(Assets/Graphics/[^"]+)(")')

HEADER = """#pragma once
#include "Utilities/FloatRect.h"

class Painter;

void Draw{name}(Painter& paint, const FloatRect& box, float time);
"""
SOURCE = """#include "{include}.h"
#include "Graphics/Paint/Painter.h"

void Draw{name}(Painter& paint, const FloatRect& box, float time) {{
    (void)time;
    paint.Circle(box.Center(), box.width * 0.5f, Fill({{255, 255, 255, 255}}));
}}
"""
REGISTRATION = """#include "{include}.h"
#include "Editor/Api/GraphicAsset.h"

namespace {{
GraphicAsset Make{name}() {{
    GraphicAsset asset;
    asset.usage = "Draw{name}(paint, box, time);";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t, float time) {{ Draw{name}(paint, box, time); }};
    return asset;
}}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&Make{name});
}} // namespace
"""


def add_parser(commands) -> None:
    command = commands.add_parser("asset", help="create and move the project's graphic assets")
    actions = command.add_subparsers(dest="asset_action", required=True, metavar="<action>")
    action = actions.add_parser("new", help="a graphic's skeleton: X.h, X.cpp and X.asset.cpp")
    action.add_argument("name", help="PascalCase name; also names the draw function")
    action.add_argument("--folder", default="", help="folder under Game/Assets/Graphics")
    actions.add_parser("mkdir", help="a folder under Game/Assets/Graphics").add_argument("folder")
    action = actions.add_parser("move", help="move an asset or folder and update the includes that name it")
    action.add_argument("source", help="asset (Enemies/Goblin) or folder (Enemies), under Game/Assets/Graphics")
    action.add_argument("destination", help="existing folder under Game/Assets/Graphics; empty for its top")


def relative_folder(text: str) -> PurePosixPath:
    """A folder path under the graphics root, or an error for one that would leave it."""
    path = PurePosixPath(text.replace("\\", "/"))
    if path.is_absolute() or ".." in path.parts or any(":" in part for part in path.parts):
        raise ToolError(f"Expected a path under {GRAPHICS.as_posix()}: {text}", EXIT_FAILED)
    return path


def new_graphic(root: Path, name: str, folder: str) -> dict:
    if not NAME.fullmatch(name):
        raise ToolError(f"Asset names are PascalCase letters and digits: {name}", EXIT_FAILED)
    relative = relative_folder(folder)
    target = root / GRAPHICS / relative
    files = {
        target / f"{name}.h": HEADER,
        target / f"{name}.cpp": SOURCE,
        target / f"{name}.asset.cpp": REGISTRATION,
    }
    existing = [path for path in files if path.exists()]
    if existing:
        raise ToolError(f"Already exists: {existing[0]}", EXIT_FAILED)
    target.mkdir(parents=True, exist_ok=True)
    include = (PurePosixPath(INCLUDE_ROOT) / relative / name).as_posix()
    for path, text in files.items():
        path.write_text(text.format(name=name, include=include), encoding="utf-8", newline="\n")
    return {"created": [path.relative_to(root).as_posix() for path in files]}


def make_folder(root: Path, folder: str) -> dict:
    relative = relative_folder(folder)
    target = root / GRAPHICS / relative
    if not relative.parts:
        raise ToolError("Expected a folder name", EXIT_FAILED)
    if target.exists():
        raise ToolError(f"Already exists: {target}", EXIT_FAILED)
    target.mkdir(parents=True)
    return {"created": [target.relative_to(root).as_posix()]}


def asset_files(folder: Path, stem: str) -> list[Path]:
    """The files of one asset: those sharing its name up to the first dot."""
    return sorted(path for path in folder.iterdir() if path.is_file() and path.name.split(".", 1)[0] == stem)


def source_files(root: Path):
    for path in root.rglob("*"):
        if path.suffix not in SOURCE_SUFFIXES or not path.is_file():
            continue
        if SKIPPED_FOLDERS.intersection(path.relative_to(root).parts[:-1]):
            continue
        yield path


def rewrite_includes(root: Path, renamed) -> list[str]:
    """Applies `renamed(include path) -> new path or None` to every literal asset include in the project."""
    changed = []
    for path in source_files(root):
        with open(path, encoding="utf-8", newline="") as file:
            text = file.read()

        def replacement(match: re.Match) -> str:
            target = renamed(match.group(2))
            return match.group(0) if target is None else match.group(1) + target + match.group(3)

        updated = INCLUDE.sub(replacement, text)
        if updated != text:
            with open(path, "w", encoding="utf-8", newline="") as file:
                file.write(updated)
            changed.append(path.relative_to(root).as_posix())
    return sorted(changed)


def move_asset(root: Path, source: str, destination: str) -> dict:
    relative = relative_folder(source)
    if not relative.parts:
        raise ToolError("Expected an asset or folder to move", EXIT_FAILED)
    target_folder = relative_folder(destination)
    graphics = root / GRAPHICS
    if not (graphics / target_folder).is_dir():
        raise ToolError(f"No such folder: {graphics / target_folder}", EXIT_FAILED)
    old_include = (PurePosixPath(INCLUDE_ROOT) / relative).as_posix()
    new_include = (PurePosixPath(INCLUDE_ROOT) / target_folder / relative.name).as_posix()
    if old_include == new_include:
        raise ToolError(f"Already there: {source}", EXIT_FAILED)

    source_path = graphics / relative
    if source_path.is_dir():
        if target_folder == relative or relative in target_folder.parents:
            raise ToolError(f"Cannot move a folder into itself: {source}", EXIT_FAILED)
        moves = [(source_path, graphics / target_folder / relative.name)]
        prefix = old_include + "/"

        def renamed(include: str) -> str | None:
            return new_include + "/" + include[len(prefix) :] if include.startswith(prefix) else None
    else:
        files = asset_files(source_path.parent, relative.name) if source_path.parent.is_dir() else []
        if not files:
            raise ToolError(f"No such asset or folder: {source}", EXIT_FAILED)
        moves = [(path, graphics / target_folder / path.name) for path in files]

        def renamed(include: str) -> str | None:
            folder, _, name = include.rpartition("/")
            same = folder == str(PurePosixPath(old_include).parent) and name.split(".", 1)[0] == relative.name
            return f"{PurePosixPath(new_include).parent}/{name}" if same else None

    taken = [target for _, target in moves if target.exists()]
    if taken:
        raise ToolError(f"Already exists: {taken[0]}", EXIT_FAILED)
    changed = rewrite_includes(root, renamed)
    for path, target in moves:
        path.rename(target)
    return {
        "moved": [[old.relative_to(root).as_posix(), new.relative_to(root).as_posix()] for old, new in moves],
        "includesUpdated": changed,
    }


def asset(project: Project, args: argparse.Namespace) -> int:
    match args.asset_action:
        case "new":
            answer = new_graphic(project.root, args.name, args.folder)
        case "mkdir":
            answer = make_folder(project.root, args.folder)
        case "move":
            answer = move_asset(project.root, args.source, args.destination)
    print(json.dumps(answer, indent=2))
    return EXIT_OK
